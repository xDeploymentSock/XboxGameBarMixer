#pragma once

#include "App.xaml.g.h"
#include <fuser/view_resume_state.h>

namespace winrt::SoftwareFuser::implementation {

struct App : AppT<App> {
    App();
    void OnLaunched(Windows::ApplicationModel::Activation::LaunchActivatedEventArgs const& args);
    void OnActivated(Windows::ApplicationModel::Activation::IActivatedEventArgs const& args);

private:
    void suspend_current_view() noexcept;
    void resume_current_view(std::uint64_t token) noexcept;
    void shutdown_current_view() noexcept;
    fuser::view_resume_state resume_state_;
    event_token resuming_token_{};
    bool resuming_registered_{};
    Microsoft::Gaming::XboxGameBar::XboxGameBarWidget widget_{nullptr};
    Windows::UI::Xaml::Window widget_window_{nullptr};
    event_token closed_token_{};
};

} // namespace winrt::SoftwareFuser::implementation
