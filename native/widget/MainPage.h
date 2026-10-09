#pragma once

#include "MainPage.g.h"
#include "../windows/D3D11Renderer.h"
#include "../streaming/OverlaySession.h"
#include <fuser/widget_layout_requests.h>
#include <fuser/black_key.h>
#include <winrt/Windows.UI.ViewManagement.h>
#include <memory>

namespace winrt::SoftwareFuser::implementation {

struct MainPage : MainPageT<MainPage> {
    MainPage();
    ~MainPage();
    void OnNavigatedTo(Windows::UI::Xaml::Navigation::NavigationEventArgs const& args);
    void save_profile_click(Windows::Foundation::IInspectable const&,
                            Windows::UI::Xaml::RoutedEventArgs const&);
    void hud_quality_click(Windows::Foundation::IInspectable const&,
                           Windows::UI::Xaml::RoutedEventArgs const&);
    void connect_click(Windows::Foundation::IInspectable const&,
                       Windows::UI::Xaml::RoutedEventArgs const&);
    void pair_click(Windows::Foundation::IInspectable const&,
                    Windows::UI::Xaml::RoutedEventArgs const&);
    void refresh_apps_click(Windows::Foundation::IInspectable const&,
                            Windows::UI::Xaml::RoutedEventArgs const&);
    void disconnect_click(Windows::Foundation::IInspectable const&,
                          Windows::UI::Xaml::RoutedEventArgs const&);
    void draw_preview_click(Windows::Foundation::IInspectable const&,
                            Windows::UI::Xaml::RoutedEventArgs const&);
    void clear_preview_click(Windows::Foundation::IInspectable const&,
                             Windows::UI::Xaml::RoutedEventArgs const&);
    void fit_monitor_click(Windows::Foundation::IInspectable const&,
                           Windows::UI::Xaml::RoutedEventArgs const&);
    void apply_dimensions_click(Windows::Foundation::IInspectable const&,
                                Windows::UI::Xaml::RoutedEventArgs const&);
    void full_screen_fit_click(Windows::Foundation::IInspectable const&,
                               Windows::UI::Xaml::RoutedEventArgs const&);
    void reset_position_click(Windows::Foundation::IInspectable const&,
                              Windows::UI::Xaml::RoutedEventArgs const&);
    void key_color_changed(Windows::Foundation::IInspectable const&,
                           Windows::UI::Xaml::Controls::SelectionChangedEventArgs const&);
    void key_settings_changed(
        Windows::Foundation::IInspectable const&,
        Windows::UI::Xaml::Controls::Primitives::RangeBaseValueChangedEventArgs const&);
    void clean_black_click(Windows::Foundation::IInspectable const&,
                           Windows::UI::Xaml::RoutedEventArgs const&);
    void exact_black_click(Windows::Foundation::IInspectable const&,
                           Windows::UI::Xaml::RoutedEventArgs const&);
    void apply_key_click(Windows::Foundation::IInspectable const&,
                         Windows::UI::Xaml::RoutedEventArgs const&);
    void settings_tab_click(Windows::Foundation::IInspectable const&,
                            Windows::UI::Xaml::RoutedEventArgs const&);
    void settings_section_changed(Windows::Foundation::IInspectable const&,
                                  Windows::UI::Xaml::Controls::SelectionChangedEventArgs const&);
    void advanced_options_changed(Windows::Foundation::IInspectable const&,
                                  Windows::UI::Xaml::RoutedEventArgs const&);
    void video_host_size_changed(Windows::Foundation::IInspectable const&,
                                 Windows::UI::Xaml::SizeChangedEventArgs const&);
    void video_layout_size_changed(Windows::Foundation::IInspectable const&,
                                   Windows::UI::Xaml::SizeChangedEventArgs const&);
    void apply_video_fit_click(Windows::Foundation::IInspectable const&,
                               Windows::UI::Xaml::RoutedEventArgs const&);
    void capture_blocking_changed(Windows::Foundation::IInspectable const&,
                                  Windows::UI::Xaml::RoutedEventArgs const&);
    void shutdown() noexcept;

private:
    [[nodiscard]] fuser::overlay_configuration read_profile(bool require_host);
    [[nodiscard]] fuser::chroma_key_settings read_key_settings();
    void save_key_settings();
    void set_black_key_preset(fuser::black_key_preset preset);
    void update_key_values();
    void select_settings_section(std::int32_t index);
    void update_settings_layout();
    void load_profile();
    void apply_capture_blocking(hstring const& trigger);
    void update_saved_profile_summary();
    void attach_renderer();
    void update_widget_state();
    void start_layout_request();
    void update_coverage();
    void log_view_geometry();
    void restore_resize_limits();
    void save_overlay_dimensions();
    void use_monitor_dimensions();
    void update_video_layout();
    fire_and_forget fit_monitor_async();
    fire_and_forget control_async(bool pairing);
    fire_and_forget connect_async();
    fire_and_forget disconnect_async();
    void set_busy(bool value);
    void update_statistics();
    void report(hstring const& message);

    fuser::overlay_configuration configuration_;
    std::shared_ptr<fuser::streaming::sunshine_control> control_;
    std::shared_ptr<fuser::streaming::overlay_session> session_;
    std::shared_ptr<fuser::windows::d3d11_renderer> renderer_;
    std::vector<fuser::host_application> applications_;
    std::string applications_host_;
    Windows::UI::Xaml::DispatcherTimer stats_timer_{nullptr};
    Windows::Graphics::Display::DisplayInformation display_{nullptr};
    Windows::UI::ViewManagement::ApplicationView application_view_{nullptr};
    bool busy_{}, streaming_{};
    bool loading_profile_{true}, fitting_monitor_{}, reset_pending_{};
    bool video_fit_enabled_{true};
    bool follow_game_bar_opacity_{};
    bool block_screen_capture_{};
    std::int32_t selected_settings_section_{};
    std::uint32_t reserved_bottom_pixels_{0};
    fuser::widget_layout_requests layout_requests_;
    std::atomic<bool> shutting_down_{};
    fuser::pipeline_statistics previous_counters_;
    fuser::monotonic_time previous_sample_{};
    unsigned int log_ticks_{};
    Windows::UI::Composition::SpriteVisual visual_{nullptr};
    Microsoft::Gaming::XboxGameBar::XboxGameBarWidget widget_{nullptr};
    event_token opacity_token_{};
    event_token mode_token_{};
    event_token click_token_{};
    event_token bounds_token_{};
    event_token pinned_token_{};
    event_token visible_token_{};
    event_token dpi_token_{};
    event_token orientation_token_{};
    event_token contents_token_{};
    event_token client_bounds_token_{};
    hstring previous_geometry_;
    std::uint64_t diagnostic_sequence_{};
};

} // namespace winrt::SoftwareFuser::implementation

namespace winrt::SoftwareFuser::factory_implementation {
struct MainPage : MainPageT<MainPage, implementation::MainPage> {};
}
