#pragma once

#include "MainPage.g.h"
#include "../windows/D3D11Renderer.h"
#include "../streaming/OverlaySession.h"
#include <memory>

namespace winrt::SoftwareFuser::implementation {

struct MainPage : MainPageT<MainPage> {
    MainPage();
    ~MainPage();
    void OnNavigatedTo(Windows::UI::Xaml::Navigation::NavigationEventArgs const& args);
    void save_profile_click(Windows::Foundation::IInspectable const&, Windows::UI::Xaml::RoutedEventArgs const&);
    void connect_click(Windows::Foundation::IInspectable const&, Windows::UI::Xaml::RoutedEventArgs const&);
    void pair_click(Windows::Foundation::IInspectable const&, Windows::UI::Xaml::RoutedEventArgs const&);
    void refresh_apps_click(Windows::Foundation::IInspectable const&, Windows::UI::Xaml::RoutedEventArgs const&);
    void disconnect_click(Windows::Foundation::IInspectable const&, Windows::UI::Xaml::RoutedEventArgs const&);
    void draw_preview_click(Windows::Foundation::IInspectable const&, Windows::UI::Xaml::RoutedEventArgs const&);
    void clear_preview_click(Windows::Foundation::IInspectable const&, Windows::UI::Xaml::RoutedEventArgs const&);
    void fit_monitor_click(Windows::Foundation::IInspectable const&, Windows::UI::Xaml::RoutedEventArgs const&);
    void cover_monitor_changed(Windows::Foundation::IInspectable const&, Windows::UI::Xaml::RoutedEventArgs const&);
    void key_color_changed(Windows::Foundation::IInspectable const&, Windows::UI::Xaml::Controls::SelectionChangedEventArgs const&);
    void video_host_size_changed(Windows::Foundation::IInspectable const&, Windows::UI::Xaml::SizeChangedEventArgs const&);
    void shutdown() noexcept;

private:
    [[nodiscard]] fuser::overlay_configuration read_profile(bool require_host);
    void load_profile();
    void attach_renderer();
    void update_widget_state();
    void schedule_monitor_fit();
    void update_coverage();
    void log_view_geometry();
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
    Windows::UI::Xaml::DispatcherTimer stats_timer_{nullptr};
    Windows::UI::Xaml::DispatcherTimer fit_timer_{nullptr};
    Windows::Graphics::Display::DisplayInformation display_{nullptr};
    bool busy_{}, streaming_{};
    bool loading_profile_{true}, fitting_monitor_{}, fit_pending_{};
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
    hstring previous_geometry_;
    std::uint64_t diagnostic_sequence_{};
};

} // namespace winrt::SoftwareFuser::implementation

namespace winrt::SoftwareFuser::factory_implementation {
struct MainPage : MainPageT<MainPage, implementation::MainPage> {};
}
