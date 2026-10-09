#include "pch.h"
#include "MainPage.h"
#include "MainPage.g.cpp"
#include "RuntimeLog.h"
#include "AppCredentials.h"
#include <fuser/application_selection.h>
#include <fuser/monitor_layout.h>
#include <fuser/video_layout.h>
#include <windows.ui.composition.interop.h>
#include <winrt/Windows.ApplicationModel.Core.h>
#include <winrt/Windows.UI.ViewManagement.h>
#include <winrt/Windows.UI.Xaml.Media.h>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <openssl/rand.h>
#include <sstream>
#include <iomanip>

namespace winrt::SoftwareFuser::implementation {
using namespace Windows::Foundation;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Hosting;
using namespace Microsoft::Gaming::XboxGameBar;

namespace {
std::uint32_t positive_number(hstring const& text) {
    const auto input = to_string(text);
    std::uint32_t value{};
    const auto [end, error] = std::from_chars(input.data(), input.data() + input.size(), value);
    if (error != std::errc{} || end != input.data() + input.size() || value == 0) {
        throw std::invalid_argument{"Enter positive whole numbers for video settings."};
    }
    return value;
}

std::uint32_t pixels(double logical_size, double scale) {
    const auto size = std::ceil(logical_size * scale);
    if (!std::isfinite(size) || size < 1.0 || size > 16384.0) {
        throw std::invalid_argument{"Widget render size is invalid."};
    }
    return static_cast<std::uint32_t>(size);
}
}

MainPage::MainPage() {
    InitializeComponent();
    load_profile();
    loading_profile_ = false;
    update_key_values();
    update_saved_profile_summary();
    select_settings_section(0);
    const auto folder = Windows::Storage::ApplicationData::Current().LocalFolder().Path();
    control_ = std::make_shared<fuser::streaming::sunshine_control>(
        std::make_shared<fuser::widget::app_credentials>(std::filesystem::path{folder.c_str()}));
    session_ =
        std::make_shared<fuser::streaming::overlay_session>(control_, [](const std::string& text) {
            fuser::widget::log(L"Transport: " + to_hstring(text));
        });
    stats_timer_ = DispatcherTimer{};
    stats_timer_.Interval(std::chrono::seconds{1});
    stats_timer_.Tick([weak = get_weak()](auto const&, auto const&) {
        if (const auto self = weak.get()) {
            self->update_statistics();
        }
    });
    Loaded([weak = get_weak()](auto const&, auto const&) {
        if (const auto self = weak.get(); self && !self->shutting_down_) {
            fuser::widget::log(L"Page loaded; video host=" +
                               to_hstring(self->VideoHost().ActualWidth()) + L"x" +
                               to_hstring(self->VideoHost().ActualHeight()));
            self->update_settings_layout();
            self->update_widget_state();
            if (self->reset_pending_) {
                self->reset_pending_ = false;
                self->layout_requests_.request(fuser::widget_layout_action::reset_position);
            }
            self->start_layout_request();
        }
    });
    fuser::widget::log(L"MainPage created.");
}

MainPage::~MainPage() {
    shutdown();
}

void MainPage::OnNavigatedTo(Windows::UI::Xaml::Navigation::NavigationEventArgs const& args) {
    widget_ = args.Parameter().try_as<XboxGameBarWidget>();
    fuser::widget::log(widget_ ? L"MainPage attached to Game Bar." : L"MainPage standalone.");
    if (widget_) {
        restore_resize_limits();
        const auto update = [weak = get_weak()](auto const&, auto const&) {
            if (const auto self = weak.get()) {
                // Game Bar callbacks are not guaranteed to use the XAML thread.
                const auto ignored = self->Dispatcher().RunAsync(
                    Windows::UI::Core::CoreDispatcherPriority::Normal, [weak] {
                        if (const auto page = weak.get(); page && !page->shutting_down_) {
                            page->update_widget_state();
                        }
                    });
                (void)ignored;
            }
        };
        opacity_token_ = widget_.RequestedOpacityChanged(update);
        mode_token_ = widget_.GameBarDisplayModeChanged(update);
        click_token_ = widget_.ClickThroughEnabledChanged(update);
        pinned_token_ = widget_.PinnedChanged(update);
        visible_token_ = widget_.VisibleChanged(update);
        bounds_token_ = widget_.WindowBoundsChanged([weak = get_weak()](auto const&, auto const&) {
            if (const auto self = weak.get()) {
                const auto ignored = self->Dispatcher().RunAsync(
                    Windows::UI::Core::CoreDispatcherPriority::Normal, [weak] {
                        if (const auto page = weak.get(); page && !page->shutting_down_) {
                            page->update_video_layout();
                            page->update_coverage();
                        }
                    });
                (void)ignored;
            }
        });
    }
    display_ = Windows::Graphics::Display::DisplayInformation::GetForCurrentView();
    const auto values = Windows::Storage::ApplicationData::Current().LocalSettings().Values();
    if (!values.HasKey(L"TaskbarHeightPixels") ||
        reserved_bottom_pixels_ >= display_.ScreenHeightInRawPixels()) {
        // The visible Game Bar client already excludes host chrome. Reserve
        // extra space only when the user explicitly requests it.
        reserved_bottom_pixels_ = 0;
    }
    TaskbarHeight().Text(to_hstring(reserved_bottom_pixels_));
    if (!values.HasKey(L"OverlayWidth") && !values.HasKey(L"OverlayHeight")) {
        use_monitor_dimensions();
    }
    const auto display_changed = [weak = get_weak()](auto const&, auto const&) {
        if (const auto self = weak.get()) {
            const auto ignored = self->Dispatcher().RunAsync(
                Windows::UI::Core::CoreDispatcherPriority::Normal, [weak] {
                    if (const auto page = weak.get(); page && !page->shutting_down_) {
                        // DPI can change without a change to the logical XAML size.
                        page->layout_requests_.invalidate();
                        page->update_video_layout();
                        page->video_host_size_changed(nullptr, nullptr);
                        if (page->fitting_monitor_) {
                            page->report(
                                L"Display changed during fitting. Click Fit my monitor again on the intended display.");
                        }
                    }
                });
            (void)ignored;
        }
    };
    dpi_token_ = display_.DpiChanged(display_changed);
    orientation_token_ = display_.OrientationChanged(display_changed);
    contents_token_ =
        Windows::Graphics::Display::DisplayInformation::DisplayContentsInvalidated(display_changed);
    application_view_ = Windows::UI::ViewManagement::ApplicationView::GetForCurrentView();
    apply_capture_blocking(L"navigation");
    client_bounds_token_ =
        application_view_.VisibleBoundsChanged([weak = get_weak()](auto const&, auto const&) {
            if (const auto self = weak.get()) {
                const auto ignored = self->Dispatcher().RunAsync(
                    Windows::UI::Core::CoreDispatcherPriority::Normal, [weak] {
                        if (const auto page = weak.get(); page && !page->shutting_down_) {
                            page->update_video_layout();
                            page->update_coverage();
                        }
                    });
                (void)ignored;
            }
        });
    update_widget_state();
    update_video_layout();
}

fuser::overlay_configuration MainPage::read_profile(bool require_host) {
    fuser::overlay_configuration result;
    result.host.address = to_string(HostAddress().Text());
    result.stream.width = positive_number(VideoWidth().Text());
    result.stream.height = positive_number(VideoHeight().Text());
    result.stream.frames_per_second = positive_number(VideoFps().Text());
    result.stream.bitrate_kbps = positive_number(VideoBitrate().Text());
    switch (VideoCodec().SelectedIndex()) {
    case 0:
        result.stream.codec = fuser::video_codec::h264;
        break;
    case 1:
        result.stream.codec = fuser::video_codec::hevc;
        break;
    default:
        throw std::invalid_argument{"Select a codec."};
    }
    result.key = read_key_settings();
    const auto issues = fuser::validate(result, require_host);
    if (!issues.empty()) {
        throw std::invalid_argument{issues.front().message};
    }
    return result;
}

fuser::chroma_key_settings MainPage::read_key_settings() {
    fuser::overlay_configuration result;
    switch (KeyColor().SelectedIndex()) {
    case 0:
        result.key.color = {0.0F, 1.0F, 0.0F};
        break;
    case 1:
        result.key.color = {1.0F, 0.0F, 1.0F};
        break;
    case 2:
        result.key.color = {0.0F, 0.0F, 0.0F};
        break;
    default:
        throw std::invalid_argument{"Select a source background."};
    }
    result.key.tolerance = static_cast<float>(KeyTolerance().Value());
    result.key.softness = static_cast<float>(KeySoftness().Value());
    result.key.opacity = static_cast<float>(KeyOpacity().Value());
    result.key.crisp_scaling = VideoScaling().SelectedIndex() == 1;
    result.key.recover_black_edges =
        KeyColor().SelectedIndex() == 2 && RecoverBlackEdges().IsChecked().Value();
    const auto issues = fuser::validate(result, false);
    if (!issues.empty()) {
        throw std::invalid_argument{issues.front().message};
    }
    return result.key;
}

void MainPage::load_profile() {
    const auto values = Windows::Storage::ApplicationData::Current().LocalSettings().Values();
    if (values.HasKey(L"FitVideoAboveTaskbar")) {
        video_fit_enabled_ = unbox_value_or<bool>(values.Lookup(L"FitVideoAboveTaskbar"), true);
    }
    if (values.HasKey(L"TaskbarHeightPixels")) {
        reserved_bottom_pixels_ =
            unbox_value_or<std::uint32_t>(values.Lookup(L"TaskbarHeightPixels"), 0);
    }
    block_screen_capture_ = values.HasKey(L"BlockScreenCapture") &&
                            unbox_value_or<bool>(values.Lookup(L"BlockScreenCapture"), false);
    BlockScreenCaptureToggle().IsOn(block_screen_capture_);
    FitVideoAboveTaskbar().IsChecked(video_fit_enabled_);
    TaskbarHeight().Text(to_hstring(reserved_bottom_pixels_));
    const auto restore_text = [&values](hstring const& name, auto const& box) {
        if (values.HasKey(name)) {
            box.Text(unbox_value_or<hstring>(values.Lookup(name), box.Text()));
        }
    };
    restore_text(L"HostAddress", HostAddress());
    restore_text(L"VideoWidth", VideoWidth());
    restore_text(L"VideoHeight", VideoHeight());
    restore_text(L"VideoFps", VideoFps());
    restore_text(L"VideoBitrate", VideoBitrate());
    restore_text(L"OverlayWidth", OverlayWidth());
    restore_text(L"OverlayHeight", OverlayHeight());
    if (values.HasKey(L"VideoCodec")) {
        const auto index = unbox_value_or<int32_t>(values.Lookup(L"VideoCodec"), 1);
        VideoCodec().SelectedIndex(index >= 0 && index <= 1 ? index : 1);
    }
    if (values.HasKey(L"KeyColor")) {
        const auto index = unbox_value_or<int32_t>(values.Lookup(L"KeyColor"), 2);
        KeyColor().SelectedIndex(index >= 0 && index <= 2 ? index : 2);
    }
    const bool black = KeyColor().SelectedIndex() == 2;
    KeyTolerance().Value(0.12);
    KeySoftness().Value(black ? 0.0 : 0.08);
    if (values.HasKey(L"ResetWidgetPosition")) {
        reset_pending_ = unbox_value_or<bool>(values.Lookup(L"ResetWidgetPosition"), false);
    }
    const auto restore_slider = [&values](hstring const& name, auto const& slider) {
        if (values.HasKey(name)) {
            const auto value = unbox_value_or<double>(values.Lookup(name), slider.Value());
            if (std::isfinite(value) && value >= 0.0 && value <= 1.0) {
                slider.Value(value);
            }
        }
    };
    restore_slider(L"KeyTolerance", KeyTolerance());
    restore_slider(L"KeySoftness", KeySoftness());
    restore_slider(L"KeyOpacity", KeyOpacity());
    if (values.HasKey(L"VideoScaling")) {
        VideoScaling().SelectedIndex(
            unbox_value_or<int32_t>(values.Lookup(L"VideoScaling"), 1) == 0 ? 0 : 1);
    }
    RecoverBlackEdges().IsChecked(black && values.HasKey(L"RecoverBlackEdges") &&
                                  unbox_value_or<bool>(values.Lookup(L"RecoverBlackEdges"), false));
    RecoverBlackEdges().IsEnabled(black);
    follow_game_bar_opacity_ = values.HasKey(L"FollowGameBarOpacity") &&
                               unbox_value_or<bool>(values.Lookup(L"FollowGameBarOpacity"), false);
    FollowGameBarOpacity().IsChecked(follow_game_bar_opacity_);
    // Older black cleanup inferred transparency from brightness. Migrate once
    // to the requested black-only behavior, without touching pairing or layout.
    if (black && values.HasKey(L"KeyColor") && !values.HasKey(L"OpaqueBlackPresetsVersion")) {
        set_black_key_preset(fuser::black_key_preset::exact);
        configuration_.key = read_key_settings();
        save_key_settings();
        fuser::widget::log(
            L"Black profile upgraded: exact removal, opaque colors, independent video opacity.");
    }
}

void MainPage::capture_blocking_changed(Windows::Foundation::IInspectable const&,
                                        Windows::UI::Xaml::RoutedEventArgs const&) {
    if (loading_profile_ || shutting_down_) {
        return;
    }
    const bool requested = BlockScreenCaptureToggle().IsOn();
    // Also suppress the synchronous event when restoring a failed save.
    if (requested == block_screen_capture_) {
        return;
    }
    try {
        const auto values = Windows::Storage::ApplicationData::Current().LocalSettings().Values();
        values.Insert(L"BlockScreenCapture", box_value(requested));
    } catch (hresult_error const& error) {
        BlockScreenCaptureToggle().IsOn(block_screen_capture_);
        CaptureBlockingStatus().Text(L"Preference could not be saved. Previous choice retained.");
        fuser::widget::log(L"Capture blocking save failed: requested=" + to_hstring(requested) +
                           L" error=" + to_hstring(static_cast<int32_t>(error.code())));
        return;
    }
    block_screen_capture_ = requested;
    apply_capture_blocking(L"toggle");
}

void MainPage::apply_capture_blocking(hstring const& trigger) {
    const auto request = L"Capture blocking: requested=" + to_hstring(block_screen_capture_) +
                         L" trigger=" + trigger;
    if (!application_view_) {
        CaptureBlockingStatus().Text(
            L"Could not apply: application view unavailable. Choice saved.");
        fuser::widget::log(request + L" view unavailable error=" +
                           to_hstring(static_cast<int32_t>(E_UNEXPECTED)));
        return;
    }
    try {
        application_view_.IsScreenCaptureEnabled(!block_screen_capture_);
        const bool enabled = application_view_.IsScreenCaptureEnabled();
        fuser::widget::log(request + L" screenCaptureEnabled=" + to_hstring(enabled));
        if (enabled != !block_screen_capture_) {
            fuser::widget::log(request + L" readback mismatch error=" +
                               to_hstring(static_cast<int32_t>(E_FAIL)));
            CaptureBlockingStatus().Text(L"Could not apply Windows setting. Choice saved.");
        } else if (block_screen_capture_) {
            CaptureBlockingStatus().Text(
                L"Windows setting applied; capture result unverified. Test your capture tool.");
        } else {
            CaptureBlockingStatus().Text(L"Off. Windows capture is enabled for this view.");
        }
    } catch (hresult_error const& error) {
        CaptureBlockingStatus().Text(L"Could not apply Windows setting. Choice saved.");
        fuser::widget::log(request + L" error=" + to_hstring(static_cast<int32_t>(error.code())));
    }
}

void MainPage::update_saved_profile_summary() {
    const auto codec = VideoCodec().SelectedIndex() == 0 ? L"H.264" : L"HEVC";
    SavedProfileText().Text(VideoWidth().Text() + L" x " + VideoHeight().Text() + L" / " +
                            VideoFps().Text() + L" FPS / " + codec + L"\n" + VideoBitrate().Text() +
                            L" kbps");
}

void MainPage::save_key_settings() {
    const auto values = Windows::Storage::ApplicationData::Current().LocalSettings().Values();
    values.Insert(L"KeyColor", box_value(KeyColor().SelectedIndex()));
    values.Insert(L"KeyTolerance", box_value(KeyTolerance().Value()));
    values.Insert(L"KeySoftness", box_value(KeySoftness().Value()));
    values.Insert(L"KeyOpacity", box_value(KeyOpacity().Value()));
    values.Insert(L"VideoScaling", box_value(VideoScaling().SelectedIndex()));
    values.Insert(L"RecoverBlackEdges", box_value(configuration_.key.recover_black_edges));
    follow_game_bar_opacity_ = FollowGameBarOpacity().IsChecked().Value();
    values.Insert(L"FollowGameBarOpacity", box_value(follow_game_bar_opacity_));
    values.Insert(L"OpaqueBlackPresetsVersion", box_value(std::uint32_t{1}));
    fuser::widget::log(L"Key settings: mode=" + to_hstring(KeyColor().SelectedIndex()) +
                       L" tolerance=" + to_hstring(configuration_.key.tolerance) + L" softness=" +
                       to_hstring(configuration_.key.softness) + L" opacity=" +
                       to_hstring(configuration_.key.opacity) + L" recover-black-edges=" +
                       to_hstring(configuration_.key.recover_black_edges) + L" crisp-scaling=" +
                       to_hstring(configuration_.key.crisp_scaling) + L" follow-game-bar-opacity=" +
                       to_hstring(follow_game_bar_opacity_));
}

void MainPage::hud_quality_click(IInspectable const&, RoutedEventArgs const&) {
    if (busy_ || shutting_down_) {
        return;
    }
    VideoWidth().Text(L"2560");
    VideoHeight().Text(L"1440");
    VideoFps().Text(L"240");
    VideoCodec().SelectedIndex(1);
    VideoBitrate().Text(L"100000");
    VideoScaling().SelectedIndex(1);
    save_profile_click(nullptr, nullptr);
    report(
        L"HUD preset saved: 1440p, HEVC, 240 requested FPS, 100 Mbps. Crisp scaling applies now; reconnect for stream changes.");
}

void MainPage::save_profile_click(IInspectable const&, RoutedEventArgs const&) {
    try {
        configuration_ = read_profile(false);
        const auto values = Windows::Storage::ApplicationData::Current().LocalSettings().Values();
        values.Insert(L"HostAddress", box_value(HostAddress().Text()));
        values.Insert(L"VideoWidth", box_value(VideoWidth().Text()));
        values.Insert(L"VideoHeight", box_value(VideoHeight().Text()));
        values.Insert(L"VideoFps", box_value(VideoFps().Text()));
        values.Insert(L"VideoBitrate", box_value(VideoBitrate().Text()));
        values.Insert(L"VideoCodec", box_value(VideoCodec().SelectedIndex()));
        save_key_settings();
        update_saved_profile_summary();
        update_widget_state();
        if (streaming_ && !busy_) {
            session_->set_key(configuration_.key);
            report(
                L"Profile saved. Chroma key settings applied; video settings take effect on the next connection.");
        } else {
            report(L"Profile saved. No connection was started.");
        }
    } catch (const hresult_error& error) {
        report(error.message());
    } catch (const std::exception& error) {
        report(to_hstring(error.what()));
    }
}

void MainPage::connect_click(IInspectable const&, RoutedEventArgs const&) {
    connect_async();
}

void MainPage::pair_click(IInspectable const&, RoutedEventArgs const&) {
    control_async(true);
}
void MainPage::refresh_apps_click(IInspectable const&, RoutedEventArgs const&) {
    control_async(false);
}
void MainPage::disconnect_click(IInspectable const&, RoutedEventArgs const&) {
    if (busy_) {
        control_->cancel();
        session_->cancel();
        report(L"Cancelling the pending action.");
    } else {
        disconnect_async();
    }
}
void MainPage::set_busy(bool value) {
    busy_ = value;
    PairButton().IsEnabled(!value && !streaming_);
    RefreshButton().IsEnabled(!value && !streaming_);
    ConnectButton().IsEnabled(!value && !streaming_);
    HostAddress().IsEnabled(!value && !streaming_);
    SourceApplication().IsEnabled(!value && !streaming_);
    CleanBlackButton().IsEnabled(!value);
    ExactBlackButton().IsEnabled(!value);
    ApplyKeyButton().IsEnabled(!value);
    SaveProfileButton().IsEnabled(!value);
    HudQualityButton().IsEnabled(!value);
    DisconnectButton().IsEnabled(value || streaming_);
    DisconnectButton().Content(box_value(value ? L"Cancel" : L"Disconnect"));
}
fire_and_forget MainPage::control_async(bool pairing) {
    const auto lifetime = get_strong();
    try {
        if (busy_ || streaming_ || shutting_down_) {
            co_return;
        }
        fuser::host_endpoint host{to_string(HostAddress().Text())};
        std::optional<fuser::host_application> previous_application;
        const auto previous_index = SourceApplication().SelectedIndex();
        if (host.address == applications_host_ && previous_index >= 0 &&
            static_cast<std::size_t>(previous_index) < applications_.size()) {
            previous_application = applications_[static_cast<std::size_t>(previous_index)];
        }
        std::string pin;
        if (pairing) {
            std::array<unsigned char, 2> random{};
            if (RAND_bytes(random.data(), 2) != 1) {
                report(L"Cannot generate a pairing PIN.");
                co_return;
            }
            const auto value = ((static_cast<unsigned int>(random[0]) << 8) | random[1]) % 10000;
            std::ostringstream text;
            text << std::setfill('0') << std::setw(4) << value;
            pin = text.str();
            PairingPinText().Visibility(Visibility::Visible);
            PairingPinText().Text(
                L"PIN: " + to_hstring(pin) +
                L"\nOn the source PC, open Sunshine's PIN page and enter this code. It expires here after two minutes.");
        }
        set_busy(true);
        control_->prepare_action();
        report(pairing ? L"Waiting for Sunshine pairing on the source PC."
                       : L"Reading Sunshine applications.");
        const auto foreground = Dispatcher();
        auto control = control_;
        std::vector<fuser::host_application> applications;
        fuser::streaming::host_information info;
        fuser::operation_result result;
        hstring error_message;
        co_await resume_background();
        try {
            if (pairing) {
                result = control->pair(host, pin);
            }
            if (result.succeeded()) {
                result = control->list_applications(host, applications);
            }
            if (result.succeeded()) {
                result = control->inspect(host, info);
            }
        } catch (const hresult_error& error) {
            error_message = error.message();
        } catch (...) {
            error_message = L"Sunshine control failed.";
        }
        if (shutting_down_) {
            co_return;
        }
        co_await resume_foreground(foreground);
        set_busy(false);
        PairingPinText().Text(L"");
        PairingPinText().Visibility(Visibility::Collapsed);
        if (shutting_down_) {
            co_return;
        }
        if (!error_message.empty()) {
            report(error_message);
            co_return;
        }
        if (!result.succeeded()) {
            report(to_hstring(result.detail));
            co_return;
        }
        applications_ = std::move(applications);
        applications_host_ = host.address;
        SourceApplication().Items().Clear();
        for (std::size_t index = 0; index < applications_.size(); ++index) {
            const bool current =
                applications_[index].id == std::to_string(info.current_application);
            SourceApplication().Items().Append(
                box_value(to_hstring(applications_[index].name + (current ? " (active)" : ""))));
        }
        const auto selected = fuser::choose_source_application(applications_, previous_application);
        SourceApplication().SelectedIndex(selected ? static_cast<int32_t>(*selected) : -1);
        Windows::Storage::ApplicationData::Current().LocalSettings().Values().Insert(
            L"HostAddress", box_value(to_hstring(host.address)));
        report(L"Paired with " + to_hstring(info.name) + L". " +
               (selected ? to_hstring(applications_[*selected].name) + L" selected. Click Connect."
                         : hstring{L"Select the source application, then click Connect."}));
    } catch (...) {
        fuser::widget::log(L"Control action could not finish; page may have closed.");
        control_->cancel();
    }
}
fire_and_forget MainPage::connect_async() {
    const auto lifetime = get_strong();
    try {
        if (busy_ || streaming_ || shutting_down_) {
            co_return;
        }
        fuser::host_application application;
        try {
            configuration_ = read_profile(true);
            if (configuration_.host.address != applications_host_) {
                report(L"The source PC changed. Refresh apps before connecting.");
                co_return;
            }
            const auto selected = SourceApplication().SelectedIndex();
            if (selected < 0 || static_cast<std::size_t>(selected) >= applications_.size()) {
                report(L"Pair or refresh apps, then select the source application.");
                co_return;
            }
            application = applications_[static_cast<std::size_t>(selected)];
            if (!renderer_) {
                attach_renderer();
            }
        } catch (const hresult_error& error) {
            report(error.message());
            co_return;
        } catch (const std::exception& error) {
            report(to_hstring(error.what()));
            co_return;
        }
        streaming_ = true; // The render worker now owns resize/draw/present.
        set_busy(true);
        report(L"Connecting to " + to_hstring(application.name) + L" (Sunshine app ID " +
               to_hstring(application.id) + L").");
        session_->prepare_action();
        const auto foreground = Dispatcher();
        auto session = session_;
        auto renderer = renderer_;
        const auto config = configuration_;
        fuser::operation_result result;
        co_await resume_background();
        try {
            result = session->begin(config, application, renderer, 0);
        } // Display refresh is unspecified.
        catch (...) {
            session->stop();
            result = {fuser::operation_code::transport_error,
                      "Streaming could not start. Check Sunshine and stored pairing credentials."};
        }
        if (shutting_down_) {
            session->stop();
            co_return;
        }
        co_await resume_foreground(foreground);
        set_busy(false);
        if (shutting_down_) {
            co_return;
        }
        if (!result.succeeded()) {
            streaming_ = false;
            set_busy(false);
            report(to_hstring(result.detail));
            co_return;
        }
        previous_counters_ = {};
        previous_sample_ = std::chrono::steady_clock::now();
        log_ticks_ = 0;
        stats_timer_.Start();
        update_statistics();
    } catch (...) {
        fuser::widget::log(L"Connect action could not finish; page may have closed.");
        session_->cancel();
    }
}
fire_and_forget MainPage::disconnect_async() {
    const auto lifetime = get_strong();
    try {
        if (busy_ || shutting_down_) {
            co_return;
        }
        set_busy(true);
        stats_timer_.Stop();
        const auto foreground = Dispatcher();
        auto session = session_;
        co_await resume_background();
        session->stop();
        if (shutting_down_) {
            co_return;
        }
        co_await resume_foreground(foreground);
        streaming_ = false;
        StreamSummaryText().Text(L"Not streaming");
        set_busy(false);
        if (shutting_down_) {
            co_return;
        }
        try {
            if (renderer_) {
                renderer_->clear();
                renderer_->present();
            }
        } catch (...) {
            renderer_.reset();
        }
        report(L"Disconnected.");
    } catch (...) {
        fuser::widget::log(L"Disconnect UI could not finish; page may have closed.");
    }
}
void MainPage::update_statistics() {
    if (!streaming_ || busy_ || shutting_down_) {
        return;
    }
    const auto state = session_->snapshot();
    const auto now = std::chrono::steady_clock::now();
    const auto seconds = std::chrono::duration<double>(now - previous_sample_).count();
    if (seconds > 0.1) {
        const auto rate = [seconds](std::uint64_t now_count, std::uint64_t previous) {
            return static_cast<double>(now_count - previous) / seconds;
        };
        std::ostringstream text;
        const auto codec = state.negotiated.codec == fuser::video_codec::hevc   ? "HEVC"
                           : state.negotiated.codec == fuser::video_codec::h264 ? "H.264"
                                                                                : "AV1";
        const auto receive_rate =
            rate(state.counters.received_frames, previous_counters_.received_frames);
        const auto decode_rate =
            rate(state.counters.decoded_frames, previous_counters_.decoded_frames);
        const auto present_rate =
            rate(state.counters.present_calls, previous_counters_.present_calls);
        text << codec << ' ' << state.negotiated.width << 'x' << state.negotiated.height
             << " / setup FPS " << state.negotiated.frames_per_second << "\n"
             << std::fixed << std::setprecision(1) << "Received units/s " << receive_rate
             << " | Decoded/s " << decode_rate << " | Present calls/s " << present_rate
             << "\nReplaced display frames " << state.counters.replaced_display_frames
             << " | Decode errors " << state.decode_errors;
        if (state.render_latency_samples) {
            text << "\nLocal callback-to-Present avg "
                 << static_cast<double>(state.render_microseconds) / 1000.0 /
                        static_cast<double>(state.render_latency_samples)
                 << " ms | max " << static_cast<double>(state.max_render_microseconds) / 1000.0
                 << " ms";
            text << std::setprecision(2) << "\nLocal latency p95/p99 <= "
                 << static_cast<double>(state.render_timing.p95_microseconds) / 1000.0 << '/'
                 << static_cast<double>(state.render_timing.p99_microseconds) / 1000.0 << " ms";
        }
        text << std::setprecision(2) << "\nAccepted-Present gaps p95/p99 <= "
             << static_cast<double>(state.present_intervals.p95_microseconds) / 1000.0 << '/'
             << static_cast<double>(state.present_intervals.p99_microseconds) / 1000.0 << " ms"
             << "\nGPU slot retries " << state.gpu_slot_retries << " | Present retries "
             << state.present_retries;
        text << "\nThis excludes host, network and monitor scanout.";
        StatsText().Text(to_hstring(text.str()));
        std::ostringstream summary;
        summary << codec << ' ' << state.negotiated.width << 'x' << state.negotiated.height << " / "
                << state.negotiated.frames_per_second << " requested FPS\n"
                << std::fixed << std::setprecision(1) << "Rx " << receive_rate << "/s | Decode "
                << decode_rate << "/s | Present " << present_rate << "/s";
        StreamSummaryText().Text(to_hstring(summary.str()));
        if (++log_ticks_ % 5 == 0) {
            // Numeric cumulative diagnostics stay in the local log. Preserve
            // zero host samples as absent/repeated rather than zero latency.
            text << "\nPacket diagnostics: frame index " << state.last_frame_number
                 << " | skipped before callback " << state.missing_frame_numbers
                 << " | peak pending decode units " << state.peak_decode_queue
                 << "\nDecode submission: units " << state.counters.received_frames
                 << " | total us " << state.decode_microseconds << " | maximum us "
                 << state.max_decode_microseconds << "\nCallback-to-Present: samples "
                 << state.render_latency_samples << " | total us " << state.render_microseconds
                 << " | maximum us " << state.max_render_microseconds << " | p95 bound us "
                 << state.render_timing.p95_microseconds << " | p99 bound us "
                 << state.render_timing.p99_microseconds << "\nPresentation gaps: samples "
                 << state.present_intervals.samples << " | p95 bound us "
                 << state.present_intervals.p95_microseconds << " | p99 bound us "
                 << state.present_intervals.p99_microseconds << " | maximum us "
                 << state.present_intervals.max_microseconds
                 << "\nRender pressure: replaced pending frames " << state.replaced_pending_frames
                 << " | GPU slot retries " << state.gpu_slot_retries << " | Present retries "
                 << state.present_retries << " | presentation wait timeouts "
                 << state.presentation_wait_timeouts << "\nReceiver timing: samples "
                 << state.receive_timing_samples << " | assembly total us "
                 << state.assembly_microseconds << " | enqueue-to-submission total us "
                 << state.queue_microseconds << " | maximum us " << state.max_queue_microseconds
                 << "\nHost processing: nonzero samples " << state.host_latency_samples
                 << " | total tenths ms " << state.host_latency_tenths_ms << " | maximum tenths ms "
                 << state.max_host_latency_tenths_ms << " | absent/repeated "
                 << state.zero_host_latency_frames;
            const auto timing = [&text](const char* label,
                                        const fuser::timing_distribution& value) {
                text << '\n'
                     << label << ": samples " << value.samples << " | total us "
                     << value.total_microseconds << " | maximum us " << value.max_microseconds
                     << " | p95 bound us " << value.p95_microseconds << " | p99 bound us "
                     << value.p99_microseconds;
            };
            text << "\nSession timing: started tick us " << state.connection_started_microseconds
                 << " | elapsed us " << static_cast<std::uint64_t>(state.seconds * 1000000.0);
            timing("Decode call", state.decode_call_timing);
            timing("Presentation ready wait", state.ready_wait_timing);
            timing("Presentation timeout wait", state.timeout_wait_timing);
            timing("Draw call", state.draw_call_timing);
            timing("Present API call", state.present_call_timing);
            text << "\nWorker activity: decode "
                 << (state.decoder_activity.observed
                         ? fuser::worker_stage_name(state.decoder_activity.stage)
                         : "not-observed")
                 << " | age us " << state.decoder_activity.age_microseconds << " | render "
                 << (state.render_activity.observed
                         ? fuser::worker_stage_name(state.render_activity.stage)
                         : "not-observed")
                 << " | age us " << state.render_activity.age_microseconds
                 << " | transport queue overflows " << state.transport_queue_overflows;
            fuser::widget::log(to_hstring(text.str()));
        }
        previous_counters_ = state.counters;
        previous_sample_ = now;
    }
    const auto status = to_hstring(state.status);
    if (!status.empty() && StatusText().Text() != status) {
        report(status);
    }
    if (state.finished) {
        disconnect_async();
    }
}

void MainPage::attach_renderer() {
    const auto display = Windows::Graphics::Display::DisplayInformation::GetForCurrentView();
    const auto scale = display.RawPixelsPerViewPixel();
    auto renderer = std::make_shared<fuser::windows::d3d11_renderer>();
    renderer->initialize(pixels(VideoHost().ActualWidth(), scale),
                         pixels(VideoHost().ActualHeight(), scale));
    fuser::widget::log(
        L"Widget associated display: raw screen " + to_hstring(display.ScreenWidthInRawPixels()) +
        L"x" + to_hstring(display.ScreenHeightInRawPixels()) + L" | scale " + to_hstring(scale));
    com_ptr<IDXGIDevice> adapter_device;
    com_ptr<IDXGIAdapter> adapter;
    DXGI_ADAPTER_DESC adapter_description{};
    if (SUCCEEDED(
            renderer->device()->QueryInterface(__uuidof(IDXGIDevice), adapter_device.put_void())) &&
        SUCCEEDED(adapter_device->GetAdapter(adapter.put())) &&
        SUCCEEDED(adapter->GetDesc(&adapter_description))) {
        fuser::widget::log(
            L"Renderer adapter: " + hstring{adapter_description.Description} + L" | vendor " +
            to_hstring(adapter_description.VendorId) + L" | device " +
            to_hstring(adapter_description.DeviceId) + L" | luid high " +
            to_hstring(static_cast<std::int32_t>(adapter_description.AdapterLuid.HighPart)) +
            L" low " +
            to_hstring(static_cast<std::uint32_t>(adapter_description.AdapterLuid.LowPart)));
    }
    const auto compositor = ElementCompositionPreview::GetElementVisual(VideoHost()).Compositor();
    const auto interop = compositor.as<ABI::Windows::UI::Composition::ICompositorInterop>();
    Windows::UI::Composition::ICompositionSurface surface{nullptr};
    check_hresult(interop->CreateCompositionSurfaceForSwapChain(
        renderer->swap_chain(),
        reinterpret_cast<ABI::Windows::UI::Composition::ICompositionSurface**>(put_abi(surface))));
    const auto visual = compositor.CreateSpriteVisual();
    visual.Size({static_cast<float>(VideoHost().ActualWidth()),
                 static_cast<float>(VideoHost().ActualHeight())});
    const auto brush = compositor.CreateSurfaceBrush(surface);
    brush.Stretch(Windows::UI::Composition::CompositionStretch::Fill);
    visual.Brush(brush);
    ElementCompositionPreview::SetElementChildVisual(VideoHost(), visual);
    visual_ = visual;
    renderer_ = std::move(renderer);
    update_widget_state();
}

void MainPage::draw_preview_click(IInspectable const&, RoutedEventArgs const&) {
    if (streaming_) {
        report(L"Disconnect before drawing a test pattern.");
        return;
    }
    fuser::widget::log(L"Draw requested; video host=" + to_hstring(VideoHost().ActualWidth()) +
                       L"x" + to_hstring(VideoHost().ActualHeight()));
    try {
        configuration_ = read_profile(false);
        fuser::widget::log(L"Preview key: mode=" + to_hstring(KeyColor().SelectedIndex()) +
                           L" tolerance=" + to_hstring(configuration_.key.tolerance) +
                           L" softness=" + to_hstring(configuration_.key.softness) + L" opacity=" +
                           to_hstring(configuration_.key.opacity));
        if (!renderer_) {
            attach_renderer();
        }
        renderer_->draw_diagnostic(configuration_.key, diagnostic_sequence_++);
        renderer_->present();
        report(L"One test frame submitted. This is not a 240 FPS measurement.");
    } catch (const hresult_error& error) {
        report(error.message());
    } catch (const std::exception& error) {
        report(to_hstring(error.what()));
    }
}

void MainPage::clear_preview_click(IInspectable const&, RoutedEventArgs const&) {
    if (streaming_) {
        report(L"Disconnect to clear the stream.");
        return;
    }
    try {
        if (renderer_) {
            renderer_->clear();
            renderer_->present();
        }
        report(L"Preview cleared.");
    } catch (const hresult_error& error) {
        report(error.message());
    }
}

void MainPage::video_host_size_changed(IInspectable const&, SizeChangedEventArgs const&) {
    if (shutting_down_) {
        return;
    }
    update_coverage();
    if (!renderer_ || VideoHost().ActualWidth() < 1.0 || VideoHost().ActualHeight() < 1.0) {
        return;
    }
    try {
        const auto scale = Windows::Graphics::Display::DisplayInformation::GetForCurrentView()
                               .RawPixelsPerViewPixel();
        visual_.Size({static_cast<float>(VideoHost().ActualWidth()),
                      static_cast<float>(VideoHost().ActualHeight())});
        if (streaming_) {
            session_->resize(pixels(VideoHost().ActualWidth(), scale),
                             pixels(VideoHost().ActualHeight(), scale));
            return;
        }
        renderer_->resize(pixels(VideoHost().ActualWidth(), scale),
                          pixels(VideoHost().ActualHeight(), scale));
        renderer_->draw_diagnostic(configuration_.key, diagnostic_sequence_++);
        renderer_->present();
    } catch (const hresult_error& error) {
        report(error.message());
    } catch (const std::exception& error) {
        report(to_hstring(error.what()));
    }
}

void MainPage::video_layout_size_changed(IInspectable const&, SizeChangedEventArgs const&) {
    if (!loading_profile_ && !shutting_down_) {
        update_settings_layout();
        update_video_layout();
    }
}

void MainPage::select_settings_section(std::int32_t index) {
    const std::array sections{ConnectionSection(), AdjustmentsSection(), AdvancedSection()};
    const std::array tabs{ConnectionTab(), AdjustmentsTab(), AdvancedTab()};
    if (index < 0 || static_cast<std::size_t>(index) >= sections.size()) {
        return;
    }
    const bool changed = selected_settings_section_ != index;
    selected_settings_section_ = index;
    for (std::size_t position = 0; position < sections.size(); ++position) {
        const bool selected = position == static_cast<std::size_t>(index);
        sections[position].Visibility(selected ? Visibility::Visible : Visibility::Collapsed);
        tabs[position].IsChecked(selected);
    }
    if (SettingsSectionPicker().SelectedIndex() != index) {
        SettingsSectionPicker().SelectedIndex(index);
    }
    if (changed) {
        SettingsScroll().ChangeView(nullptr, 0.0, nullptr, true);
    }
}

void MainPage::settings_tab_click(IInspectable const& sender, RoutedEventArgs const&) {
    if (loading_profile_ || shutting_down_) {
        return;
    }
    const auto tag =
        unbox_value_or<hstring>(sender.as<Controls::Primitives::ToggleButton>().Tag(), L"");
    if (tag.size() == 1 && tag[0] >= L'0' && tag[0] <= L'2') {
        select_settings_section(static_cast<std::int32_t>(tag[0] - L'0'));
    }
}

void MainPage::settings_section_changed(IInspectable const&,
                                        Controls::SelectionChangedEventArgs const&) {
    if (!loading_profile_ && !shutting_down_) {
        select_settings_section(SettingsSectionPicker().SelectedIndex());
    }
}

void MainPage::advanced_options_changed(IInspectable const&, RoutedEventArgs const&) {
    if (loading_profile_ || shutting_down_) {
        return;
    }
    StreamOptionsPanel().Visibility(
        StreamOptionsToggle().IsChecked().Value() ? Visibility::Visible : Visibility::Collapsed);
    KeyOptionsPanel().Visibility(KeyOptionsToggle().IsChecked().Value() ? Visibility::Visible
                                                                        : Visibility::Collapsed);
    PlacementOptionsPanel().Visibility(
        PlacementOptionsToggle().IsChecked().Value() ? Visibility::Visible : Visibility::Collapsed);
}

void MainPage::update_settings_layout() {
    const auto width = VideoLayoutRoot().ActualWidth();
    const auto height = VideoLayoutRoot().ActualHeight();
    if (width < 1.0 || height < 1.0) {
        return;
    }
    constexpr double outer_margin = 24.0;
    constexpr double card_insets = 34.0; // Padding and border on both sides.
    const auto card_width = std::min(420.0, std::max(1.0, width - outer_margin));
    const auto card_height = std::min(660.0, std::max(1.0, height - outer_margin));
    SettingsCard().Width(card_width);
    SettingsCard().Height(card_height);
    // A shallow window can scroll the complete menu instead of clipping actions.
    MenuLayout().Height(std::max(480.0, card_height - card_insets));
    const bool narrow = card_width < 380.0;
    SettingsTabs().Visibility(narrow ? Visibility::Collapsed : Visibility::Visible);
    SettingsSectionPicker().Visibility(narrow ? Visibility::Visible : Visibility::Collapsed);
}

void MainPage::apply_video_fit_click(IInspectable const&, RoutedEventArgs const&) {
    if (loading_profile_ || shutting_down_) {
        return;
    }
    try {
        const auto text = to_string(TaskbarHeight().Text());
        const auto first = text.find_first_not_of(" \t\r\n");
        const auto last = text.find_last_not_of(" \t\r\n");
        const auto input =
            first == std::string::npos ? std::string{} : text.substr(first, last - first + 1);
        const auto inset = input == "0" ? 0U : fuser::parse_widget_dimension(input);
        const auto display = Windows::Graphics::Display::DisplayInformation::GetForCurrentView();
        if (inset >= display.ScreenHeightInRawPixels()) {
            throw std::invalid_argument{
                "Taskbar height must be smaller than this monitor's height."};
        }
        reserved_bottom_pixels_ = inset;
        const auto checked = FitVideoAboveTaskbar().IsChecked();
        video_fit_enabled_ = checked && checked.Value();
        TaskbarHeight().Text(to_hstring(inset));
        const auto values = Windows::Storage::ApplicationData::Current().LocalSettings().Values();
        values.Insert(L"TaskbarHeightPixels", box_value(inset));
        values.Insert(L"FitVideoAboveTaskbar", box_value(video_fit_enabled_));
        update_video_layout();
        // No host resize, source crop, stream renegotiation or reconnect here.
        report(
            video_fit_enabled_
                ? L"Video fit applied. The whole feed fills the available area above the taskbar."
                : L"Video now fills the widget's full client area.");
    } catch (const hresult_error& error) {
        report(error.message());
    } catch (const std::exception& error) {
        report(to_hstring(error.what()));
    }
}

void MainPage::update_video_layout() {
    if (shutting_down_) {
        return;
    }
    if (VideoLayoutRoot().ActualWidth() < 1.0 || VideoLayoutRoot().ActualHeight() < 1.0) {
        return;
    }
    try {
        const auto display = Windows::Graphics::Display::DisplayInformation::GetForCurrentView();
        const auto scale = display.RawPixelsPerViewPixel();
        const auto monitor = fuser::monitor_view_extent(
            display.ScreenWidthInRawPixels(), display.ScreenHeightInRawPixels(), scale);
        // Game Bar's IPC bounds include its frame and can lag the hosted view
        // when pinning. Fit against the actual XAML client origin instead.
        const auto bounds = Window::Current().CoreWindow().Bounds();
        const auto origin =
            VideoLayoutRoot().TransformToVisual(nullptr).TransformPoint({0.0F, 0.0F});
        const auto area =
            video_fit_enabled_
                ? fuser::usable_video_rectangle({bounds.X + origin.X,
                                                 bounds.Y + origin.Y,
                                                 VideoLayoutRoot().ActualWidth(),
                                                 VideoLayoutRoot().ActualHeight()},
                                                monitor.width,
                                                monitor.height,
                                                scale,
                                                reserved_bottom_pixels_)
                : std::optional<fuser::video_rectangle>{
                      {0, 0, VideoLayoutRoot().ActualWidth(), VideoLayoutRoot().ActualHeight()}};
        VideoHost().Visibility(area ? Visibility::Visible : Visibility::Collapsed);
        VideoHost().HorizontalAlignment(HorizontalAlignment::Left);
        VideoHost().VerticalAlignment(VerticalAlignment::Top);
        const auto rectangle = area.value_or(fuser::video_rectangle{});
        VideoHost().Margin({rectangle.x, rectangle.y, 0, 0});
        VideoHost().Width(rectangle.width);
        VideoHost().Height(rectangle.height);
        const auto clip = Windows::UI::Xaml::Media::RectangleGeometry{};
        clip.Rect(
            {0, 0, static_cast<float>(rectangle.width), static_cast<float>(rectangle.height)});
        VideoHost().Clip(clip);
        const auto message =
            !area
                ? hstring{L"No usable video area at this position. Move the widget above the taskbar."}
                : L"Whole feed fills " + to_hstring(std::round(rectangle.width * scale)) + L" x " +
                      to_hstring(std::round(rectangle.height * scale)) +
                      L" pixels. Taskbar reservation: " +
                      to_hstring(video_fit_enabled_ ? reserved_bottom_pixels_ : 0U) + L" px.";
        if (VideoFitText().Text() != message) {
            VideoFitText().Text(message);
            fuser::widget::log(L"Video fit: " + message + L" local origin=" +
                               to_hstring(rectangle.x) + L"," + to_hstring(rectangle.y));
        }
    } catch (const hresult_error& error) {
        fuser::widget::log(L"Video layout: " + error.message());
    } catch (const std::exception& error) {
        fuser::widget::log(L"Video layout: " + to_hstring(error.what()));
    }
}

void MainPage::fit_monitor_click(IInspectable const&, RoutedEventArgs const&) {
    if (loading_profile_ || shutting_down_) {
        return;
    }
    if (!widget_) {
        report(L"Open Software Fuser through Win+G before fitting its monitor.");
        return;
    }
    try {
        use_monitor_dimensions();
        save_overlay_dimensions();
        reset_pending_ = false;
        Windows::Storage::ApplicationData::Current().LocalSettings().Values().Insert(
            L"ResetWidgetPosition", box_value(false));
        layout_requests_.request(fuser::widget_layout_action::fit_monitor);
        report(
            fitting_monitor_
                ? L"Fit queued after the current request; your latest action takes priority."
                : L"Fitting this monitor now. Drag or resize the widget if Game Bar constrains it.");
        start_layout_request();
    } catch (const hresult_error& error) {
        report(error.message());
    }
}

void MainPage::use_monitor_dimensions() {
    const auto display = Windows::Graphics::Display::DisplayInformation::GetForCurrentView();
    OverlayWidth().Text(to_hstring(display.ScreenWidthInRawPixels()));
    OverlayHeight().Text(to_hstring(display.ScreenHeightInRawPixels()));
}

void MainPage::save_overlay_dimensions() {
    const auto values = Windows::Storage::ApplicationData::Current().LocalSettings().Values();
    values.Insert(L"OverlayWidth", box_value(OverlayWidth().Text()));
    values.Insert(L"OverlayHeight", box_value(OverlayHeight().Text()));
    values.Insert(L"ResetWidgetPosition", box_value(false));
    reset_pending_ = false;
}

void MainPage::apply_dimensions_click(IInspectable const&, RoutedEventArgs const&) {
    if (loading_profile_ || shutting_down_) {
        return;
    }
    if (!widget_) {
        report(L"Open Software Fuser through Win+G before applying overlay dimensions.");
        return;
    }
    try {
        const fuser::widget_pixel_extent extent{
            fuser::parse_widget_dimension(to_string(OverlayWidth().Text())),
            fuser::parse_widget_dimension(to_string(OverlayHeight().Text()))};
        const auto scale = Windows::Graphics::Display::DisplayInformation::GetForCurrentView()
                               .RawPixelsPerViewPixel();
        (void)fuser::widget_view_extent(extent.width, extent.height, scale);
        OverlayWidth().Text(to_hstring(extent.width));
        OverlayHeight().Text(to_hstring(extent.height));
        save_overlay_dimensions();
        // Snapshot the typed dimensions; edits made during the await affect only
        // a later click, and must not change this request's meaning.
        layout_requests_.apply_dimensions(extent);
        report(fitting_monitor_ ? L"Dimensions queued; your latest action takes priority."
                                : L"Applying overlay dimensions now.");
        start_layout_request();
    } catch (const hresult_error& error) {
        report(error.message());
    } catch (const std::exception& error) {
        report(to_hstring(error.what()));
    }
}

void MainPage::full_screen_fit_click(IInspectable const&, RoutedEventArgs const&) {
    if (loading_profile_ || shutting_down_) {
        return;
    }
    if (!widget_) {
        report(L"Open Software Fuser through Win+G before trying full-screen fit.");
        return;
    }
    try {
        use_monitor_dimensions();
        save_overlay_dimensions();
        layout_requests_.request(fuser::widget_layout_action::full_screen_fit);
        report(fitting_monitor_ ? L"Full-screen fit queued; your latest action takes priority."
                                : L"Trying Windows full-screen mode for this widget.");
        start_layout_request();
    } catch (const hresult_error& error) {
        report(error.message());
    }
}

void MainPage::reset_position_click(IInspectable const&, RoutedEventArgs const&) {
    if (loading_profile_ || shutting_down_) {
        return;
    }
    try {
        const auto values = Windows::Storage::ApplicationData::Current().LocalSettings().Values();
        values.Insert(L"CoverMonitor", box_value(false));
        values.Insert(L"ResetWidgetPosition", box_value(true));
        reset_pending_ = false;
        layout_requests_.request(fuser::widget_layout_action::reset_position);
        restore_resize_limits();
        if (!widget_) {
            report(
                L"Position reset saved. Reopen Software Fuser through the Game Bar widget menu.");
        } else if (fitting_monitor_) {
            report(L"Reset queued after the current layout request.");
        } else {
            start_layout_request();
        }
    } catch (const hresult_error& error) {
        report(error.message());
    }
}

void MainPage::key_color_changed(IInspectable const&, Controls::SelectionChangedEventArgs const&) {
    if (loading_profile_ || shutting_down_) {
        return;
    }
    const bool black = KeyColor().SelectedIndex() == 2;
    KeyTolerance().Value(black ? 0.0 : 0.12);
    KeySoftness().Value(black ? 0.0 : 0.08);
    RecoverBlackEdges().IsChecked(false);
    RecoverBlackEdges().IsEnabled(black);
    update_key_values();
}

void MainPage::update_key_values() {
    std::ostringstream values;
    values << std::fixed << std::setprecision(3) << "Tolerance " << KeyTolerance().Value()
           << " / softness " << KeySoftness().Value() << " / opacity " << KeyOpacity().Value();
    KeyValuesText().Text(to_hstring(values.str()));
}

void MainPage::key_settings_changed(IInspectable const&,
                                    Controls::Primitives::RangeBaseValueChangedEventArgs const&) {
    if (!loading_profile_ && !shutting_down_) {
        update_key_values();
    }
}

void MainPage::set_black_key_preset(fuser::black_key_preset preset) {
    const auto key = fuser::black_key_settings(read_key_settings(), preset);
    KeyColor().SelectedIndex(2);
    KeyTolerance().Value(key.tolerance);
    KeySoftness().Value(key.softness);
    KeyOpacity().Value(key.opacity);
    RecoverBlackEdges().IsChecked(key.recover_black_edges);
    FollowGameBarOpacity().IsChecked(false);
}

void MainPage::clean_black_click(IInspectable const&, RoutedEventArgs const&) {
    if (busy_ || shutting_down_) {
        return;
    }
    set_black_key_preset(fuser::black_key_preset::noise_cutoff);
    apply_key_click(nullptr, nullptr);
}

void MainPage::exact_black_click(IInspectable const&, RoutedEventArgs const&) {
    if (busy_ || shutting_down_) {
        return;
    }
    set_black_key_preset(fuser::black_key_preset::exact);
    apply_key_click(nullptr, nullptr);
}

void MainPage::apply_key_click(IInspectable const&, RoutedEventArgs const&) {
    if (busy_ || shutting_down_) {
        return;
    }
    try {
        configuration_.key = read_key_settings();
        save_key_settings();
        update_key_values();
        update_widget_state();
        if (streaming_) {
            session_->set_key(configuration_.key);
            report(L"Key settings saved and applied to the live video.");
        } else {
            report(L"Key settings saved. Connect or draw a test pattern to preview them.");
        }
    } catch (const hresult_error& error) {
        report(error.message());
    } catch (const std::exception& error) {
        report(to_hstring(error.what()));
    }
}

void MainPage::restore_resize_limits() {
    if (!widget_) {
        return;
    }
    widget_.MinWindowSize({240.0F, 240.0F});
    widget_.MaxWindowSize({7680.0F, 4320.0F});
    widget_.HorizontalResizeSupported(true);
    widget_.VerticalResizeSupported(true);
}

void MainPage::start_layout_request() {
    if (loading_profile_ || shutting_down_ || fitting_monitor_ || !widget_ || !widget_.Visible() ||
        !layout_requests_.has_pending()) {
        return;
    }
    fit_monitor_async();
}

void MainPage::update_coverage() {
    if (loading_profile_ || shutting_down_ || !widget_) {
        return;
    }
    log_view_geometry();
    try {
        const auto display = Windows::Graphics::Display::DisplayInformation::GetForCurrentView();
        const auto scale = display.RawPixelsPerViewPixel();
        const auto expected = fuser::monitor_view_extent(
            display.ScreenWidthInRawPixels(), display.ScreenHeightInRawPixels(), scale);
        const auto bounds = Window::Current().CoreWindow().Bounds();
        const auto origin = VideoHost().TransformToVisual(nullptr).TransformPoint({0.0F, 0.0F});
        const bool sized =
            fuser::matches_monitor_extent(bounds.Width, bounds.Height, expected, scale) &&
            fuser::matches_monitor_extent(
                VideoHost().ActualWidth(), VideoHost().ActualHeight(), expected, scale);
        const bool aligned = sized && fuser::matches_monitor_bounds(bounds.X + origin.X,
                                                                    bounds.Y + origin.Y,
                                                                    VideoHost().ActualWidth(),
                                                                    VideoHost().ActualHeight(),
                                                                    expected,
                                                                    scale);
        const auto gaps = fuser::uncovered_monitor_edges(bounds.X + origin.X,
                                                         bounds.Y + origin.Y,
                                                         VideoHost().ActualWidth(),
                                                         VideoHost().ActualHeight(),
                                                         expected,
                                                         scale);
        const auto prefix = video_fit_enabled_ ? hstring{L"Usable-area video fit enabled. "}
                            : aligned          ? hstring{L"Monitor bounds match. "}
                            : sized ? hstring{L"Monitor size matched; positioning needed. "}
                                    : hstring{L"Manual adjustment available. "};
        const auto message =
            prefix + L"Video area " + to_hstring(std::round(VideoHost().ActualWidth() * scale)) +
            L" x " + to_hstring(std::round(VideoHost().ActualHeight() * scale)) + L" / monitor " +
            to_hstring(display.ScreenWidthInRawPixels()) + L" x " +
            to_hstring(display.ScreenHeightInRawPixels()) + L". Position " +
            to_hstring(std::round((bounds.X + origin.X) * scale)) + L", " +
            to_hstring(std::round((bounds.Y + origin.Y) * scale)) + L" pixels." +
            L" Edge gaps (px): left " + to_hstring(std::round(gaps.left)) + L", top " +
            to_hstring(std::round(gaps.top)) + L", right " + to_hstring(std::round(gaps.right)) +
            L", bottom " + to_hstring(std::round(gaps.bottom)) + L"." +
            (video_fit_enabled_
                 ? L" The full source frame is scaled inside the visible area above the taskbar; move or resize the widget manually if more area is needed."
             : aligned ? L" Check all four outer white edges, including over the taskbar."
             : sized
                 ? L" Drag the title bar to align all four outer edges, including over the taskbar."
             : gaps.top > 1.0
                 ? L" Try full-screen fit for the top gap, or move the widget upward and resize the edges manually."
                 : L" Click Fit my monitor or Apply dimensions, then adjust the edges manually if needed.");
        if (CoverageText().Text() != message) {
            CoverageText().Text(message);
            fuser::widget::log(message + L" Bounds: x=" + to_hstring(bounds.X) + L" y=" +
                               to_hstring(bounds.Y) + L" width=" + to_hstring(bounds.Width) +
                               L" height=" + to_hstring(bounds.Height) + L" scale=" +
                               to_hstring(scale));
        }
    } catch (const hresult_error& error) {
        fuser::widget::log(L"Coverage query: " + error.message());
    } catch (const std::exception& error) {
        fuser::widget::log(to_hstring(error.what()));
    }
}

void MainPage::log_view_geometry() {
    try {
        const auto rectangle_text = [](Rect const& rectangle) {
            return L"x=" + to_hstring(rectangle.X) + L" y=" + to_hstring(rectangle.Y) + L" width=" +
                   to_hstring(rectangle.Width) + L" height=" + to_hstring(rectangle.Height);
        };
        const auto client = Window::Current().CoreWindow().Bounds();
        const auto visible =
            Windows::UI::ViewManagement::ApplicationView::GetForCurrentView().VisibleBounds();
        const auto origin = VideoHost().TransformToVisual(nullptr).TransformPoint({0.0F, 0.0F});
        const auto geometry = L"View geometry: widget(" + rectangle_text(widget_.WindowBounds()) +
                              L") client(" + rectangle_text(client) + L") visible(" +
                              rectangle_text(visible) + L") video-local x=" + to_hstring(origin.X) +
                              L" y=" + to_hstring(origin.Y) + L" width=" +
                              to_hstring(VideoHost().ActualWidth()) + L" height=" +
                              to_hstring(VideoHost().ActualHeight());
        if (geometry != previous_geometry_) {
            previous_geometry_ = geometry;
            fuser::widget::log(geometry);
            try {
                const auto title =
                    Windows::ApplicationModel::Core::CoreApplication::GetCurrentView().TitleBar();
                const auto view = Windows::UI::ViewManagement::ApplicationView::GetForCurrentView();
                fuser::widget::log(L"View chrome: titleHeight=" + to_hstring(title.Height()) +
                                   L" titleVisible=" + to_hstring(title.IsVisible()) +
                                   L" extended=" + to_hstring(title.ExtendViewIntoTitleBar()) +
                                   L" fullScreen=" + to_hstring(view.IsFullScreenMode()));
            } catch (const hresult_error& error) {
                fuser::widget::log(L"View chrome query: " + error.message());
            }
        }
    } catch (const hresult_error& error) {
        fuser::widget::log(L"View geometry query: " + error.message());
    }
}

fire_and_forget MainPage::fit_monitor_async() {
    const auto lifetime = get_strong();
    const auto foreground = Dispatcher();
    if (shutting_down_ || fitting_monitor_ || !widget_ || !widget_.Visible()) {
        co_return;
    }
    const auto request = layout_requests_.take();
    if (!request) {
        co_return;
    }
    fitting_monitor_ = true;
    const bool resetting = request->action == fuser::widget_layout_action::reset_position;
    const bool custom = request->action == fuser::widget_layout_action::apply_dimensions;
    const bool full_screen = request->action == fuser::widget_layout_action::full_screen_fit;
    try {
        const auto widget = widget_;
        const auto display = Windows::Graphics::Display::DisplayInformation::GetForCurrentView();
        const auto scale = display.RawPixelsPerViewPixel();
        const auto extent = fuser::monitor_view_extent(
            display.ScreenWidthInRawPixels(), display.ScreenHeightInRawPixels(), scale);
        const auto custom_extent =
            custom ? fuser::widget_view_extent(request->pixels.width, request->pixels.height, scale)
                   : extent;
        const Size requested = !resetting
                                   ? Size{custom_extent.width, custom_extent.height}
                                   : Size{std::clamp(extent.width - 80.0F, 240.0F, 480.0F),
                                          std::clamp(extent.height - 120.0F, 240.0F, 700.0F)};
        restore_resize_limits();
        const auto current_request = [&] {
            return !shutting_down_ && layout_requests_.is_current(*request) && widget_ &&
                   widget_.Visible();
        };
        const hstring kind = resetting     ? L"reset"
                             : custom      ? L"custom dimensions"
                             : full_screen ? L"full-screen fit"
                                           : L"immediate fit";
        fuser::widget::log(L"Layout request: " + kind + L" revision=" +
                           to_hstring(request->revision) + L" requested=" +
                           to_hstring(requested.Width) + L"x" + to_hstring(requested.Height));
        bool resized{};
        if (full_screen) {
            // Test one supported Windows API, independently of Game Bar's
            // resize/centering policy. Acceptance alone is not coverage proof.
            const auto view = Windows::UI::ViewManagement::ApplicationView::GetForCurrentView();
            resized = view.TryEnterFullScreenMode();
            fuser::widget::log(L"Full-screen request: accepted=" + to_hstring(resized) +
                               L" fullScreen=" + to_hstring(view.IsFullScreenMode()));
        } else {
            try {
                const auto view = Windows::UI::ViewManagement::ApplicationView::GetForCurrentView();
                if (view.IsFullScreenMode()) {
                    view.ExitFullScreenMode();
                }
            } catch (const hresult_error& error) {
                // A host without ApplicationView full-screen support must still
                // be able to use Game Bar's normal resize and Reset APIs.
                fuser::widget::log(L"Full-screen exit unavailable: " + error.message());
            }
            resized = co_await widget.TryResizeWindowAsync(requested);
        }
        if (current_request()) {
            fuser::widget::log(L"Layout request result: " + kind + L" accepted=" +
                               to_hstring(resized));
            log_view_geometry();
            // Fit keeps manual placement intact. Centering can shrink or shift
            // the hosted surface to avoid Game Bar chrome; only Reset asks for it.
            if (resetting) {
                co_await widget.CenterWindowAsync();
            }
            // Allow the hosted view to receive its layout change before checking it.
            co_await resume_after(std::chrono::milliseconds{150});
            co_await resume_foreground(foreground);
            if (current_request()) {
                update_coverage();
                fuser::widget::log(L"Layout settled: " + kind + L" accepted=" +
                                   to_hstring(resized) + L" requested=" +
                                   to_hstring(requested.Width) + L"x" +
                                   to_hstring(requested.Height));
                const bool size_matched =
                    fuser::matches_monitor_extent(VideoLayoutRoot().ActualWidth(),
                                                  VideoLayoutRoot().ActualHeight(),
                                                  {requested.Width, requested.Height},
                                                  scale);
                if (resetting) {
                    if (size_matched) {
                        Windows::Storage::ApplicationData::Current()
                            .LocalSettings()
                            .Values()
                            .Insert(L"ResetWidgetPosition", box_value(false));
                        report(
                            L"Movable window restored. Drag its title bar, or click Fit my monitor to resize now.");
                    } else {
                        report(
                            L"Game Bar declined the smaller window. Resize manually, or close and reopen to retry the saved reset.");
                    }
                } else if (full_screen) {
                    const auto bounds = Window::Current().CoreWindow().Bounds();
                    const auto origin =
                        VideoHost().TransformToVisual(nullptr).TransformPoint({0.0F, 0.0F});
                    const bool aligned = fuser::matches_monitor_bounds(bounds.X + origin.X,
                                                                       bounds.Y + origin.Y,
                                                                       VideoHost().ActualWidth(),
                                                                       VideoHost().ActualHeight(),
                                                                       extent,
                                                                       scale);
                    report(
                        aligned
                            ? L"Full-screen bounds match this monitor. Pin and check all four outer edges. Reset exits full screen."
                        : !resized
                            ? L"This Game Bar host declined Windows full-screen mode. Your placement was kept. Use Apply dimensions, then move upward and resize manually; watch the edge-gap values."
                            : L"Windows accepted full-screen mode, but the overlay still has an edge gap. Check the gap values and adjust manually; Reset exits full screen.");
                } else if (custom) {
                    const auto requested_text = to_hstring(request->pixels.width) + L" x " +
                                                to_hstring(request->pixels.height);
                    report(
                        size_matched
                            ? L"Overlay dimensions applied: " + requested_text +
                                  L" pixels. Position is unchanged; check the edge gaps."
                            : L"Game Bar constrained the requested " + requested_text +
                                  L" pixel overlay. Actual size and edge gaps are shown below; drag or resize manually.");
                } else {
                    report(
                        video_fit_enabled_
                            ? size_matched
                                  ? L"Widget size applied. The full feed fills the usable area above the taskbar."
                                  : L"Game Bar kept its current widget size. The full feed fits the usable area above the taskbar; move or resize the widget manually to enlarge it."
                        : size_matched
                            ? L"Monitor size applied. Drag the title bar if an outer edge misses the screen or taskbar. Automatic fitting is off."
                            : L"Game Bar constrained the resize. Drag the title bar and resize edges to cover the monitor, including the taskbar. Automatic fitting is off.");
                }
            }
        }
    } catch (const hresult_error& error) {
        report(
            full_screen
                ? L"Full-screen fit is unavailable in this Game Bar host: " + error.message() +
                      L" Use Apply dimensions and adjust manually; the edge gaps show what remains."
                : error.message());
    } catch (const std::exception& error) {
        report(to_hstring(error.what()));
    }
    fitting_monitor_ = false;
    start_layout_request();
}

void MainPage::update_widget_state() {
    if (shutting_down_) {
        return;
    }
    const bool pinned_only =
        widget_ && widget_.GameBarDisplayMode() == XboxGameBarDisplayMode::PinnedOnly;
    SettingsCard().Visibility(pinned_only ? Visibility::Collapsed : Visibility::Visible);
    // Keep the settings card aligned with Game Bar's opacity preference.
    // Video follows it only when explicitly requested, to preserve HUD colors.
    const auto opacity = widget_ ? static_cast<float>(widget_.RequestedOpacity()) : 1.0F;
    const auto card_opacity = fuser::video_visual_opacity(opacity, true);
    const auto video_opacity = fuser::video_visual_opacity(opacity, follow_game_bar_opacity_);
    SettingsCard().Opacity(card_opacity);
    if (visual_) {
        visual_.Opacity(video_opacity);
    }
    WidgetStateText().Text(
        widget_
            ? (widget_.ClickThroughEnabled() ? L"Game Bar click-through enabled."
                                             : L"Game Bar click-through disabled.")
            : L"Standalone settings view. Open through Game Bar for pinning and click-through.");
    WidgetStateText().Text(WidgetStateText().Text() + L" Video surface opacity: " +
                           to_hstring(std::round(video_opacity * 100.0F)) +
                           L"%. HUD opacity is set in Adjustments.");
    if (widget_) {
        fuser::widget::log(L"Widget state: pinned=" + to_hstring(widget_.Pinned()) + L" visible=" +
                           to_hstring(widget_.Visible()) + L" mode=" +
                           to_hstring(static_cast<int>(widget_.GameBarDisplayMode())) +
                           L" requestedOpacity=" + to_hstring(widget_.RequestedOpacity()) +
                           L" clickThrough=" + to_hstring(widget_.ClickThroughEnabled()) +
                           L" videoOpacity=" + to_hstring(video_opacity) + L" menuVisible=" +
                           to_hstring(SettingsCard().Visibility() == Visibility::Visible) +
                           L" menuOpacity=" + to_hstring(SettingsCard().Opacity()));
        layout_requests_.visibility_changed(widget_.Visible());
        start_layout_request();
    }
    update_video_layout();
    update_coverage();
}

void MainPage::report(hstring const& message) {
    StatusText().Text(message);
    fuser::widget::log(message);
}

void MainPage::shutdown() noexcept {
    // Shutdown can run again when the last coroutine reference is released
    // on its worker. A closed page must never touch XAML a second time.
    if (shutting_down_.exchange(true)) {
        return;
    }
    fuser::widget::log(L"MainPage shutting down.");
    try {
        if (stats_timer_) {
            stats_timer_.Stop();
        }
    } catch (...) {
        OutputDebugStringW(L"Software Fuser: timer cleanup encountered an error.\n");
    }
    layout_requests_.invalidate();
    if (control_) {
        control_->cancel();
    }
    if (session_) {
        session_->cancel();
        if (!busy_) {
            session_->stop();
        }
    }
    try {
        if (widget_) {
            widget_.RequestedOpacityChanged(opacity_token_);
            widget_.GameBarDisplayModeChanged(mode_token_);
            widget_.ClickThroughEnabledChanged(click_token_);
            widget_.WindowBoundsChanged(bounds_token_);
            widget_.PinnedChanged(pinned_token_);
            widget_.VisibleChanged(visible_token_);
            widget_ = nullptr;
        }
        if (display_) {
            display_.DpiChanged(dpi_token_);
            display_.OrientationChanged(orientation_token_);
            Windows::Graphics::Display::DisplayInformation::DisplayContentsInvalidated(
                contents_token_);
            display_ = nullptr;
        }
        if (application_view_) {
            application_view_.VisibleBoundsChanged(client_bounds_token_);
            application_view_ = nullptr;
        }
        ElementCompositionPreview::SetElementChildVisual(VideoHost(), nullptr);
        visual_ = nullptr;
    } catch (...) {
        OutputDebugStringW(L"Software Fuser: composition cleanup encountered an error.\n");
    }
    renderer_.reset();
}

} // namespace winrt::SoftwareFuser::implementation
