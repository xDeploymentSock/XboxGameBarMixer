#include "pch.h"
#include "MainPage.h"
#include "MainPage.g.cpp"
#include "RuntimeLog.h"
#include "AppCredentials.h"
#include <fuser/monitor_layout.h>
#include <windows.ui.composition.interop.h>
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
    const auto folder = Windows::Storage::ApplicationData::Current().LocalFolder().Path();
    control_ = std::make_shared<fuser::streaming::sunshine_control>(
        std::make_shared<fuser::widget::app_credentials>(std::filesystem::path{folder.c_str()}));
    session_ = std::make_shared<fuser::streaming::overlay_session>(control_, [](const std::string& text) {
        fuser::widget::log(L"Transport: " + to_hstring(text));
    });
    stats_timer_ = DispatcherTimer{};
    stats_timer_.Interval(std::chrono::seconds{1});
    stats_timer_.Tick([weak = get_weak()](auto const&, auto const&) {
        if (const auto self = weak.get()) { self->update_statistics(); }
    });
    fit_timer_ = DispatcherTimer{};
    fit_timer_.Interval(std::chrono::milliseconds{200});
    fit_timer_.Tick([weak = get_weak()](auto const&, auto const&) {
        if (const auto self = weak.get()) {
            self->fit_timer_.Stop();
            self->fit_monitor_async();
        }
    });
    Loaded([weak = get_weak()](auto const&, auto const&) {
        if (const auto self = weak.get()) {
            fuser::widget::log(L"Page loaded; video host=" + to_hstring(self->VideoHost().ActualWidth())
                + L"x" + to_hstring(self->VideoHost().ActualHeight()));
            self->schedule_monitor_fit();
        }
    });
    fuser::widget::log(L"MainPage created.");
}

MainPage::~MainPage() { shutdown(); }

void MainPage::OnNavigatedTo(Windows::UI::Xaml::Navigation::NavigationEventArgs const& args) {
    widget_ = args.Parameter().try_as<XboxGameBarWidget>();
    fuser::widget::log(widget_ ? L"MainPage attached to Game Bar." : L"MainPage standalone.");
    if (widget_) {
        const auto update = [weak = get_weak()](auto const&, auto const&) {
            if (const auto self = weak.get()) {
                // Game Bar callbacks are not guaranteed to use the XAML thread.
                const auto ignored = self->Dispatcher().RunAsync(
                    Windows::UI::Core::CoreDispatcherPriority::Normal, [weak] {
                        if (const auto page = weak.get()) {
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
                        if (const auto page = weak.get()) { page->update_coverage(); }
                    });
                (void)ignored;
            }
        });
    }
    display_ = Windows::Graphics::Display::DisplayInformation::GetForCurrentView();
    const auto display_changed = [weak = get_weak()](auto const&, auto const&) {
        if (const auto self = weak.get()) {
            const auto ignored = self->Dispatcher().RunAsync(
                Windows::UI::Core::CoreDispatcherPriority::Normal, [weak] {
                    if (const auto page = weak.get()) {
                        // DPI can change without a change to the logical XAML size.
                        ++page->layout_revision_;
                        page->video_host_size_changed(nullptr, nullptr);
                        page->schedule_monitor_fit();
                    }
                });
            (void)ignored;
        }
    };
    dpi_token_ = display_.DpiChanged(display_changed);
    orientation_token_ = display_.OrientationChanged(display_changed);
    contents_token_ = Windows::Graphics::Display::DisplayInformation::DisplayContentsInvalidated(display_changed);
    update_widget_state();
}

fuser::overlay_configuration MainPage::read_profile(bool require_host) {
    fuser::overlay_configuration result;
    result.host.address = to_string(HostAddress().Text());
    result.stream.width = positive_number(VideoWidth().Text());
    result.stream.height = positive_number(VideoHeight().Text());
    result.stream.frames_per_second = positive_number(VideoFps().Text());
    result.stream.bitrate_kbps = positive_number(VideoBitrate().Text());
    switch (VideoCodec().SelectedIndex()) {
    case 0: result.stream.codec = fuser::video_codec::h264; break;
    case 1: result.stream.codec = fuser::video_codec::hevc; break;
    default: throw std::invalid_argument{"Select a codec."};
    }
    switch (KeyColor().SelectedIndex()) {
    case 0: result.key.color = {0.0F, 1.0F, 0.0F}; break;
    case 1: result.key.color = {1.0F, 0.0F, 1.0F}; break;
    case 2: result.key.color = {0.0F, 0.0F, 0.0F}; break;
    default: throw std::invalid_argument{"Select a source background."};
    }
    result.key.tolerance = static_cast<float>(KeyTolerance().Value());
    result.key.softness = static_cast<float>(KeySoftness().Value());
    result.key.opacity = static_cast<float>(KeyOpacity().Value());
    const auto issues = fuser::validate(result, require_host);
    if (!issues.empty()) {
        throw std::invalid_argument{issues.front().message};
    }
    return result;
}

void MainPage::load_profile() {
    const auto values = Windows::Storage::ApplicationData::Current().LocalSettings().Values();
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
    if (values.HasKey(L"VideoCodec")) {
        const auto index = unbox_value_or<int32_t>(values.Lookup(L"VideoCodec"), 1);
        VideoCodec().SelectedIndex(index >= 0 && index <= 1 ? index : 1);
    }
    if (values.HasKey(L"KeyColor")) {
        const auto index = unbox_value_or<int32_t>(values.Lookup(L"KeyColor"), 2);
        KeyColor().SelectedIndex(index >= 0 && index <= 2 ? index : 2);
    }
    const bool black = KeyColor().SelectedIndex() == 2;
    KeyTolerance().Value(black ? 0.0 : 0.12);
    KeySoftness().Value(black ? 0.0 : 0.08);
    if (values.HasKey(L"CoverMonitor")) {
        CoverMonitor().IsChecked(unbox_value_or<bool>(values.Lookup(L"CoverMonitor"), true));
    }
    if (values.HasKey(L"ResetWidgetPosition")) {
        reset_pending_ = unbox_value_or<bool>(values.Lookup(L"ResetWidgetPosition"), false);
        if (reset_pending_) { CoverMonitor().IsChecked(false); }
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
        values.Insert(L"KeyColor", box_value(KeyColor().SelectedIndex()));
        values.Insert(L"KeyTolerance", box_value(KeyTolerance().Value()));
        values.Insert(L"KeySoftness", box_value(KeySoftness().Value()));
        values.Insert(L"KeyOpacity", box_value(KeyOpacity().Value()));
        if (streaming_ && !busy_) {
            session_->set_key(configuration_.key);
            report(L"Profile saved. Chroma key settings applied; video settings take effect on the next connection.");
        } else { report(L"Profile saved. No connection was started."); }
    } catch (const hresult_error& error) { report(error.message()); }
      catch (const std::exception& error) { report(to_hstring(error.what())); }
}

void MainPage::connect_click(IInspectable const&, RoutedEventArgs const&) {
    connect_async();
}

void MainPage::pair_click(IInspectable const&, RoutedEventArgs const&) { control_async(true); }
void MainPage::refresh_apps_click(IInspectable const&, RoutedEventArgs const&) { control_async(false); }
void MainPage::disconnect_click(IInspectable const&, RoutedEventArgs const&) {
    if (busy_) { control_->cancel(); session_->cancel(); report(L"Cancelling the pending action."); }
    else { disconnect_async(); }
}
void MainPage::set_busy(bool value) {
    busy_ = value;
    PairButton().IsEnabled(!value && !streaming_);
    RefreshButton().IsEnabled(!value && !streaming_);
    ConnectButton().IsEnabled(!value && !streaming_);
    HostAddress().IsEnabled(!value && !streaming_);
    SourceApplication().IsEnabled(!value && !streaming_);
}
fire_and_forget MainPage::control_async(bool pairing) {
    const auto lifetime = get_strong();
    try {
    if (busy_ || streaming_ || shutting_down_) { co_return; }
    fuser::host_endpoint host{to_string(HostAddress().Text())};
    std::string pin;
    if (pairing) {
        std::array<unsigned char, 2> random{};
        if (RAND_bytes(random.data(), 2) != 1) { report(L"Cannot generate a pairing PIN."); co_return; }
        const auto value = ((static_cast<unsigned int>(random[0]) << 8) | random[1]) % 10000;
        std::ostringstream text;
        text << std::setfill('0') << std::setw(4) << value;
        pin = text.str();
        PairingPinText().Text(L"PIN: " + to_hstring(pin) + L"\nOn the source PC, open Sunshine's PIN page and enter this code. It expires here after two minutes.");
    }
    set_busy(true);
    control_->prepare_action();
    report(pairing ? L"Waiting for Sunshine pairing on the source PC." : L"Reading Sunshine applications.");
    const auto foreground = Dispatcher();
    auto control = control_;
    std::vector<fuser::host_application> applications;
    fuser::streaming::host_information info;
    fuser::operation_result result;
    hstring error_message;
    co_await resume_background();
    try {
        if (pairing) { result = control->pair(host, pin); }
        if (result.succeeded()) { result = control->list_applications(host, applications); }
        if (result.succeeded()) { result = control->inspect(host, info); }
    } catch (const hresult_error& error) { error_message = error.message(); }
      catch (...) { error_message = L"Sunshine control failed."; }
    if (shutting_down_) { co_return; }
    co_await resume_foreground(foreground);
    set_busy(false);
    PairingPinText().Text(L"");
    if (shutting_down_) { co_return; }
    if (!error_message.empty()) { report(error_message); co_return; }
    if (!result.succeeded()) { report(to_hstring(result.detail)); co_return; }
    applications_ = std::move(applications);
    SourceApplication().Items().Clear();
    int current_index = -1;
    for (std::size_t index = 0; index < applications_.size(); ++index) {
        const bool current = applications_[index].id == std::to_string(info.current_application);
        SourceApplication().Items().Append(box_value(to_hstring(applications_[index].name + (current ? " (active)" : ""))));
        if (current) { current_index = static_cast<int>(index); }
    }
    SourceApplication().SelectedIndex(current_index >= 0 ? current_index : applications_.size() == 1 ? 0 : -1);
    Windows::Storage::ApplicationData::Current().LocalSettings().Values().Insert(L"HostAddress", box_value(to_hstring(host.address)));
    report(L"Paired with " + to_hstring(info.name) + L". Select the HUD application and click Connect. The active source application is preserved.");
    } catch (...) { fuser::widget::log(L"Control action could not finish; page may have closed."); control_->cancel(); }
}
fire_and_forget MainPage::connect_async() {
    const auto lifetime = get_strong();
    try {
    if (busy_ || streaming_ || shutting_down_) { co_return; }
    fuser::host_application application;
    try {
        configuration_ = read_profile(true);
        const auto selected = SourceApplication().SelectedIndex();
        if (selected < 0 || static_cast<std::size_t>(selected) >= applications_.size()) {
            report(L"Pair or refresh apps, then select the source application."); co_return;
        }
        application = applications_[static_cast<std::size_t>(selected)];
        if (!renderer_) { attach_renderer(); }
    } catch (const hresult_error& error) { report(error.message()); co_return; }
      catch (const std::exception& error) { report(to_hstring(error.what())); co_return; }
    streaming_ = true; // The render worker now owns resize/draw/present.
    set_busy(true);
    report(L"Connecting to Sunshine.");
    session_->prepare_action();
    const auto foreground = Dispatcher();
    auto session = session_;
    auto renderer = renderer_;
    const auto config = configuration_;
    fuser::operation_result result;
    co_await resume_background();
    try { result = session->begin(config, application, renderer, 0); } // Display refresh is unspecified.
    catch (...) { session->stop(); result = {fuser::operation_code::transport_error, "Streaming could not start. Check Sunshine and stored pairing credentials."}; }
    if (shutting_down_) { session->stop(); co_return; }
    co_await resume_foreground(foreground);
    set_busy(false);
    if (shutting_down_) { co_return; }
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
    } catch (...) { fuser::widget::log(L"Connect action could not finish; page may have closed."); session_->cancel(); }
}
fire_and_forget MainPage::disconnect_async() {
    const auto lifetime = get_strong();
    try {
    if (busy_ || shutting_down_) { co_return; }
    set_busy(true);
    stats_timer_.Stop();
    const auto foreground = Dispatcher();
    auto session = session_;
    co_await resume_background();
    session->stop();
    if (shutting_down_) { co_return; }
    co_await resume_foreground(foreground);
    streaming_ = false;
    set_busy(false);
    if (shutting_down_) { co_return; }
    try { if (renderer_) { renderer_->clear(); renderer_->present(); } }
    catch (...) { renderer_.reset(); }
    report(L"Disconnected.");
    } catch (...) { fuser::widget::log(L"Disconnect UI could not finish; page may have closed."); }
}
void MainPage::update_statistics() {
    if (!streaming_ || busy_ || shutting_down_) { return; }
    const auto state = session_->snapshot();
    const auto now = std::chrono::steady_clock::now();
    const auto seconds = std::chrono::duration<double>(now - previous_sample_).count();
    if (seconds > 0.1) {
        const auto rate = [seconds](std::uint64_t now_count, std::uint64_t previous) {
            return static_cast<double>(now_count - previous) / seconds;
        };
        std::ostringstream text;
        const auto codec = state.negotiated.codec == fuser::video_codec::hevc ? "HEVC" :
            state.negotiated.codec == fuser::video_codec::h264 ? "H.264" : "AV1";
        text << codec << ' ' << state.negotiated.width << 'x' << state.negotiated.height << " / setup FPS " << state.negotiated.frames_per_second
             << "\n" << std::fixed << std::setprecision(1)
             << "Received units/s " << rate(state.counters.received_frames, previous_counters_.received_frames)
             << " | Decoded/s " << rate(state.counters.decoded_frames, previous_counters_.decoded_frames)
             << " | Present calls/s " << rate(state.counters.present_calls, previous_counters_.present_calls)
             << "\nReplaced display frames " << state.counters.replaced_display_frames
             << " | Decode errors " << state.decode_errors << "\nPresent calls do not measure monitor scanout.";
        StatsText().Text(to_hstring(text.str()));
        if (++log_ticks_ % 5 == 0) {
            // Numeric cumulative diagnostics stay in the local log. Preserve
            // zero host samples as absent/repeated rather than zero latency.
            text << "\nPacket diagnostics: frame index " << state.last_frame_number
                 << " | skipped before callback " << state.missing_frame_numbers
                 << " | peak pending decode units " << state.peak_decode_queue
                 << "\nDecode submission: units " << state.counters.received_frames
                 << " | total us " << state.decode_microseconds
                 << " | maximum us " << state.max_decode_microseconds
                 << "\nReceiver timing: samples " << state.receive_timing_samples
                 << " | assembly total us " << state.assembly_microseconds
                 << " | enqueue-to-submission total us " << state.queue_microseconds
                 << " | maximum us " << state.max_queue_microseconds
                 << "\nHost processing: nonzero samples " << state.host_latency_samples
                 << " | total tenths ms " << state.host_latency_tenths_ms
                 << " | maximum tenths ms " << state.max_host_latency_tenths_ms
                 << " | absent/repeated " << state.zero_host_latency_frames;
            fuser::widget::log(to_hstring(text.str()));
        }
        previous_counters_ = state.counters;
        previous_sample_ = now;
    }
    const auto status = to_hstring(state.status);
    if (!status.empty() && StatusText().Text() != status) { report(status); }
    if (state.finished) { disconnect_async(); }
}

void MainPage::attach_renderer() {
    const auto display = Windows::Graphics::Display::DisplayInformation::GetForCurrentView();
    const auto scale = display.RawPixelsPerViewPixel();
    auto renderer = std::make_shared<fuser::windows::d3d11_renderer>();
    renderer->initialize(pixels(VideoHost().ActualWidth(), scale),
                         pixels(VideoHost().ActualHeight(), scale));
    fuser::widget::log(L"Widget associated display: raw screen " + to_hstring(display.ScreenWidthInRawPixels())
        + L"x" + to_hstring(display.ScreenHeightInRawPixels()) + L" | scale " + to_hstring(scale));
    com_ptr<IDXGIDevice> adapter_device;
    com_ptr<IDXGIAdapter> adapter;
    DXGI_ADAPTER_DESC adapter_description{};
    if (SUCCEEDED(renderer->device()->QueryInterface(__uuidof(IDXGIDevice), adapter_device.put_void()))
        && SUCCEEDED(adapter_device->GetAdapter(adapter.put()))
        && SUCCEEDED(adapter->GetDesc(&adapter_description))) {
        fuser::widget::log(L"Renderer adapter: " + hstring{adapter_description.Description}
            + L" | vendor " + to_hstring(adapter_description.VendorId)
            + L" | device " + to_hstring(adapter_description.DeviceId)
            + L" | luid high " + to_hstring(static_cast<std::int32_t>(adapter_description.AdapterLuid.HighPart))
            + L" low " + to_hstring(static_cast<std::uint32_t>(adapter_description.AdapterLuid.LowPart)));
    }
    const auto compositor = ElementCompositionPreview::GetElementVisual(VideoHost()).Compositor();
    const auto interop = compositor.as<ABI::Windows::UI::Composition::ICompositorInterop>();
    Windows::UI::Composition::ICompositionSurface surface{nullptr};
    check_hresult(interop->CreateCompositionSurfaceForSwapChain(renderer->swap_chain(),
        reinterpret_cast<ABI::Windows::UI::Composition::ICompositionSurface**>(put_abi(surface))));
    const auto visual = compositor.CreateSpriteVisual();
    visual.Size({static_cast<float>(VideoHost().ActualWidth()), static_cast<float>(VideoHost().ActualHeight())});
    visual.Brush(compositor.CreateSurfaceBrush(surface));
    ElementCompositionPreview::SetElementChildVisual(VideoHost(), visual);
    visual_ = visual;
    renderer_ = std::move(renderer);
    update_widget_state();
}

void MainPage::draw_preview_click(IInspectable const&, RoutedEventArgs const&) {
    if (streaming_) { report(L"Disconnect before drawing a test pattern."); return; }
    fuser::widget::log(L"Draw requested; video host=" + to_hstring(VideoHost().ActualWidth())
        + L"x" + to_hstring(VideoHost().ActualHeight()));
    try {
        configuration_ = read_profile(false);
        fuser::widget::log(L"Preview key: mode=" + to_hstring(KeyColor().SelectedIndex())
            + L" tolerance=" + to_hstring(configuration_.key.tolerance)
            + L" softness=" + to_hstring(configuration_.key.softness)
            + L" opacity=" + to_hstring(configuration_.key.opacity));
        if (!renderer_) {
            attach_renderer();
        }
        renderer_->draw_diagnostic(configuration_.key, diagnostic_sequence_++);
        renderer_->present();
        report(L"One test frame submitted. This is not a 240 FPS measurement.");
    } catch (const hresult_error& error) { report(error.message()); }
      catch (const std::exception& error) { report(to_hstring(error.what())); }
}

void MainPage::clear_preview_click(IInspectable const&, RoutedEventArgs const&) {
    if (streaming_) { report(L"Disconnect to clear the stream."); return; }
    try {
        if (renderer_) {
            renderer_->clear();
            renderer_->present();
        }
        report(L"Preview cleared.");
    } catch (const hresult_error& error) { report(error.message()); }
}

void MainPage::video_host_size_changed(IInspectable const&, SizeChangedEventArgs const&) {
    update_coverage();
    if (!renderer_ || VideoHost().ActualWidth() < 1.0 || VideoHost().ActualHeight() < 1.0) {
        return;
    }
    try {
        const auto scale = Windows::Graphics::Display::DisplayInformation::GetForCurrentView().RawPixelsPerViewPixel();
        visual_.Size({static_cast<float>(VideoHost().ActualWidth()), static_cast<float>(VideoHost().ActualHeight())});
        if (streaming_) {
            session_->resize(pixels(VideoHost().ActualWidth(), scale), pixels(VideoHost().ActualHeight(), scale));
            return;
        }
        renderer_->resize(pixels(VideoHost().ActualWidth(), scale), pixels(VideoHost().ActualHeight(), scale));
        renderer_->draw_diagnostic(configuration_.key, diagnostic_sequence_++);
        renderer_->present();
    } catch (const hresult_error& error) { report(error.message()); }
      catch (const std::exception& error) { report(to_hstring(error.what())); }
}

void MainPage::fit_monitor_click(IInspectable const&, RoutedEventArgs const&) {
    if (!CoverMonitor().IsChecked().Value()) { CoverMonitor().IsChecked(true); }
    else { schedule_monitor_fit(); }
    if (widget_ && widget_.GameBarDisplayMode() != XboxGameBarDisplayMode::PinnedOnly) {
        report(L"Pin the widget and close Game Bar to fit the overlay to this monitor.");
    }
}

void MainPage::reset_position_click(IInspectable const&, RoutedEventArgs const&) {
    if (loading_profile_ || shutting_down_) { return; }
    try {
        CoverMonitor().IsChecked(false);
        const auto values = Windows::Storage::ApplicationData::Current().LocalSettings().Values();
        values.Insert(L"CoverMonitor", box_value(false));
        values.Insert(L"ResetWidgetPosition", box_value(true));
        reset_pending_ = true;
        ++layout_revision_;
        fit_timer_.Stop();
        fit_pending_ = false;
        restore_resize_limits();
        if (!widget_) {
            report(L"Position reset saved. Reopen Software Fuser through the Game Bar widget menu.");
        } else if (fitting_monitor_) {
            report(L"Reset queued after the current layout request.");
        } else {
            fit_monitor_async();
        }
    } catch (const hresult_error& error) { report(error.message()); }
}

void MainPage::key_color_changed(IInspectable const&, Controls::SelectionChangedEventArgs const&) {
    if (loading_profile_ || shutting_down_) { return; }
    const bool black = KeyColor().SelectedIndex() == 2;
    KeyTolerance().Value(black ? 0.0 : 0.12);
    KeySoftness().Value(black ? 0.0 : 0.08);
}

void MainPage::cover_monitor_changed(IInspectable const&, RoutedEventArgs const&) {
    if (loading_profile_ || shutting_down_) { return; }
    try {
        ++layout_revision_;
        Windows::Storage::ApplicationData::Current().LocalSettings().Values().Insert(
            L"CoverMonitor", box_value(CoverMonitor().IsChecked().Value()));
        if (CoverMonitor().IsChecked().Value()) {
            reset_pending_ = false;
            Windows::Storage::ApplicationData::Current().LocalSettings().Values().Insert(
                L"ResetWidgetPosition", box_value(false));
            schedule_monitor_fit();
        }
        else {
            fit_timer_.Stop();
            fit_pending_ = false;
            restore_resize_limits();
            update_coverage();
        }
    } catch (const hresult_error& error) { report(error.message()); }
}

void MainPage::restore_resize_limits() {
    if (!widget_) { return; }
    widget_.MinWindowSize({240.0F, 240.0F});
    widget_.MaxWindowSize({7680.0F, 4320.0F});
    widget_.HorizontalResizeSupported(true);
    widget_.VerticalResizeSupported(true);
}

void MainPage::schedule_monitor_fit() {
    if (loading_profile_ || shutting_down_ || !widget_ || !widget_.Visible()
        || (!reset_pending_ && !settings_fit_pending_ && !CoverMonitor().IsChecked().Value())) { return; }
    if (fitting_monitor_) { fit_pending_ = true; return; }
    fit_timer_.Stop();
    fit_timer_.Start();
}

void MainPage::update_coverage() {
    if (loading_profile_ || shutting_down_ || !widget_) { return; }
    log_view_geometry();
    try {
        const auto display = Windows::Graphics::Display::DisplayInformation::GetForCurrentView();
        const auto scale = display.RawPixelsPerViewPixel();
        const auto expected = fuser::monitor_view_extent(
            display.ScreenWidthInRawPixels(), display.ScreenHeightInRawPixels(), scale);
        const auto bounds = widget_.WindowBounds();
        const bool matched = fuser::matches_monitor_extent(bounds.Width, bounds.Height, expected, scale)
            && fuser::matches_monitor_extent(VideoHost().ActualWidth(), VideoHost().ActualHeight(), expected, scale);
        const bool foreground_fit = CoverMonitor().IsChecked().Value()
            && widget_.GameBarDisplayMode() != XboxGameBarDisplayMode::PinnedOnly;
        const auto prefix = foreground_fit ? hstring{L"Movable settings window. "}
            : !CoverMonitor().IsChecked().Value() ? hstring{L"Manual placement. "}
            : matched ? hstring{L"Monitor size matched. "} : hstring{L"Monitor coverage constrained. "};
        const auto message = prefix
            + L"Video area " + to_hstring(std::round(VideoHost().ActualWidth() * scale))
            + L" x " + to_hstring(std::round(VideoHost().ActualHeight() * scale))
            + L" / monitor " + to_hstring(display.ScreenWidthInRawPixels())
            + L" x " + to_hstring(display.ScreenHeightInRawPixels())
            + (foreground_fit ? L". Pin and close Game Bar to fit the overlay."
                : !CoverMonitor().IsChecked().Value() ? L". Enable monitor coverage to fit when pinned."
                : matched ? L". Check all four corner markers."
                : L". Pin and close Game Bar to retry without its menu.");
        if (CoverageText().Text() != message) {
            CoverageText().Text(message);
            fuser::widget::log(message + L" Bounds: x=" + to_hstring(bounds.X)
                + L" y=" + to_hstring(bounds.Y) + L" width=" + to_hstring(bounds.Width)
                + L" height=" + to_hstring(bounds.Height) + L" scale=" + to_hstring(scale));
        }
    } catch (const hresult_error& error) { fuser::widget::log(L"Coverage query: " + error.message()); }
      catch (const std::exception& error) { fuser::widget::log(to_hstring(error.what())); }
}

void MainPage::log_view_geometry() {
    try {
        const auto rectangle_text = [](Rect const& rectangle) {
            return L"x=" + to_hstring(rectangle.X) + L" y=" + to_hstring(rectangle.Y)
                + L" width=" + to_hstring(rectangle.Width) + L" height=" + to_hstring(rectangle.Height);
        };
        const auto client = Window::Current().CoreWindow().Bounds();
        const auto visible = Windows::UI::ViewManagement::ApplicationView::GetForCurrentView().VisibleBounds();
        const auto origin = VideoHost().TransformToVisual(nullptr).TransformPoint({0.0F, 0.0F});
        const auto geometry = L"View geometry: widget(" + rectangle_text(widget_.WindowBounds())
            + L") client(" + rectangle_text(client) + L") visible(" + rectangle_text(visible)
            + L") video-local x=" + to_hstring(origin.X) + L" y=" + to_hstring(origin.Y)
            + L" width=" + to_hstring(VideoHost().ActualWidth())
            + L" height=" + to_hstring(VideoHost().ActualHeight());
        if (geometry != previous_geometry_) {
            previous_geometry_ = geometry;
            fuser::widget::log(geometry);
        }
    } catch (const hresult_error& error) { fuser::widget::log(L"View geometry query: " + error.message()); }
}

fire_and_forget MainPage::fit_monitor_async() {
    const auto lifetime = get_strong();
    const auto foreground = Dispatcher();
    if (shutting_down_ || fitting_monitor_ || !widget_ || !widget_.Visible()
        || (!reset_pending_ && !settings_fit_pending_ && !CoverMonitor().IsChecked().Value())) { co_return; }
    fitting_monitor_ = true;
    fit_pending_ = false;
    settings_fit_pending_ = false;
    const bool resetting = reset_pending_;
    const auto revision = layout_revision_;
    try {
        const auto widget = widget_;
        const bool full_monitor = !resetting && CoverMonitor().IsChecked().Value()
            && widget.Pinned() && widget.GameBarDisplayMode() == XboxGameBarDisplayMode::PinnedOnly;
        const auto display = Windows::Graphics::Display::DisplayInformation::GetForCurrentView();
        const auto scale = display.RawPixelsPerViewPixel();
        const auto extent = fuser::monitor_view_extent(
            display.ScreenWidthInRawPixels(), display.ScreenHeightInRawPixels(), scale);
        const Size requested = full_monitor ? Size{extent.width, extent.height}
            : Size{std::clamp(extent.width - 80.0F, 240.0F, 480.0F),
                   std::clamp(extent.height - 120.0F, 240.0F, 700.0F)};
        restore_resize_limits();
        if (full_monitor) {
            // Keep the pin/title controls reachable in foreground. Exact size
            // constraints apply only after Game Bar hides its own chrome.
            widget.MaxWindowSize(requested);
            widget.MinWindowSize(requested);
            widget.HorizontalResizeSupported(false);
            widget.VerticalResizeSupported(false);
        }
        const auto current_request = [&] {
            return !shutting_down_ && revision == layout_revision_ && widget_ && widget_.Visible();
        };
        const hstring kind = resetting ? L"reset" : full_monitor ? L"pinned overlay" : L"settings";
        fuser::widget::log(L"Layout request: " + kind + L" revision=" + to_hstring(revision)
            + L" requested=" + to_hstring(requested.Width) + L"x" + to_hstring(requested.Height));
        const int attempts = full_monitor ? 2 : 1;
        for (int attempt = 0; attempt < attempts; ++attempt) {
            const auto resized = co_await widget.TryResizeWindowAsync(requested);
            if (!current_request()) { break; }
            fuser::widget::log(L"Layout before centering: " + kind + L" resizeAccepted=" + to_hstring(resized));
            log_view_geometry();
            co_await widget.CenterWindowAsync();
            // Allow the hosted view to receive its layout change before checking it.
            co_await resume_after(std::chrono::milliseconds{150});
            co_await resume_foreground(foreground);
            if (!current_request()) { break; }
            update_coverage();
            fuser::widget::log(L"Layout after centering: " + kind + L" resizeAccepted=" + to_hstring(resized)
                + L" requested=" + to_hstring(requested.Width) + L"x" + to_hstring(requested.Height));
            const bool size_matched = fuser::matches_monitor_extent(VideoHost().ActualWidth(), VideoHost().ActualHeight(),
                {requested.Width, requested.Height}, scale);
            if (resetting) {
                if (size_matched) {
                    Windows::Storage::ApplicationData::Current().LocalSettings().Values().Insert(
                        L"ResetWidgetPosition", box_value(false));
                    report(L"Movable window restored. Drag its title bar to move it; enable monitor coverage to fit when pinned.");
                } else {
                    report(L"Game Bar declined the smaller window. Close and reopen the widget to retry the saved reset.");
                }
            }
            if (size_matched) { break; }
        }
    } catch (const hresult_error& error) { report(error.message()); }
      catch (const std::exception& error) { report(to_hstring(error.what())); }
    // A newer reset or mode transition keeps its request. An unchanged failed
    // reset stays saved for reopening, without an automatic retry loop.
    if (resetting && revision == layout_revision_) { reset_pending_ = false; }
    fitting_monitor_ = false;
    if (fit_pending_ || reset_pending_ || settings_fit_pending_) { schedule_monitor_fit(); }
}

void MainPage::update_widget_state() {
    const bool pinned_only = widget_ && widget_.GameBarDisplayMode() == XboxGameBarDisplayMode::PinnedOnly;
    SettingsCard().Visibility(pinned_only ? Visibility::Collapsed : Visibility::Visible);
    // This SDK/runtime reports 1.0 in foreground and 0.85 when pinned. Match
    // Microsoft's transparency sample and XAML's normalized opacity contract.
    const auto opacity = widget_ ? static_cast<float>(widget_.RequestedOpacity()) : 1.0F;
    if (visual_) {
        visual_.Opacity(std::clamp(opacity, 0.0F, 1.0F));
    }
    WidgetStateText().Text(widget_
        ? (widget_.ClickThroughEnabled() ? L"Game Bar click-through enabled." : L"Game Bar click-through disabled.")
        : L"Standalone settings view. Open through Game Bar for pinning and click-through.");
    if (widget_) {
        fuser::widget::log(L"Widget state: pinned=" + to_hstring(widget_.Pinned())
            + L" visible=" + to_hstring(widget_.Visible())
            + L" mode=" + to_hstring(static_cast<int>(widget_.GameBarDisplayMode()))
            + L" requestedOpacity=" + to_hstring(widget_.RequestedOpacity())
            + L" clickThrough=" + to_hstring(widget_.ClickThroughEnabled()));
        const int mode = static_cast<int>(widget_.GameBarDisplayMode());
        const bool visible = widget_.Visible();
        const bool first_activation = layout_mode_ == -1;
        const bool transition = mode != layout_mode_ || (visible && !layout_visible_);
        layout_mode_ = mode;
        layout_visible_ = visible;
        if (transition) {
            ++layout_revision_;
            settings_fit_pending_ = visible && !pinned_only
                && (first_activation || CoverMonitor().IsChecked().Value());
            if (!pinned_only) { restore_resize_limits(); }
            schedule_monitor_fit();
        }
    }
    update_coverage();
}

void MainPage::report(hstring const& message) {
    StatusText().Text(message);
    fuser::widget::log(message);
}

void MainPage::shutdown() noexcept {
    fuser::widget::log(L"MainPage shutting down.");
    shutting_down_ = true;
    if (stats_timer_) { stats_timer_.Stop(); }
    if (fit_timer_) { fit_timer_.Stop(); }
    if (control_) { control_->cancel(); }
    if (session_) {
        session_->cancel();
        if (!busy_) { session_->stop(); }
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
            Windows::Graphics::Display::DisplayInformation::DisplayContentsInvalidated(contents_token_);
            display_ = nullptr;
        }
        ElementCompositionPreview::SetElementChildVisual(VideoHost(), nullptr);
        visual_ = nullptr;
    } catch (...) {
        OutputDebugStringW(L"Software Fuser: composition cleanup encountered an error.\n");
    }
    renderer_.reset();
}

} // namespace winrt::SoftwareFuser::implementation
