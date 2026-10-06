#pragma once

#include "App.xaml.g.h"

namespace winrt::SoftwareFuser::implementation {

struct App : AppT<App> {
    App();
    void OnLaunched(Windows::ApplicationModel::Activation::LaunchActivatedEventArgs const& args);
    void OnActivated(Windows::ApplicationModel::Activation::IActivatedEventArgs const& args);

private:
    void dispatch_coverage_probe(bool pinned, bool center);
    void shutdown_current_view() noexcept;
    Microsoft::Gaming::XboxGameBar::XboxGameBarWidget widget_{nullptr};
    Windows::UI::Xaml::Window widget_window_{nullptr};
    event_token closed_token_{};
};

} // namespace winrt::SoftwareFuser::implementation
