#include "pch.h"
#include "App.h"
#include "MainPage.h"
#include "RuntimeLog.h"

namespace winrt::SoftwareFuser::implementation {
using namespace Windows::ApplicationModel::Activation;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using Microsoft::Gaming::XboxGameBar::XboxGameBarWidget;
using Microsoft::Gaming::XboxGameBar::XboxGameBarWidgetActivatedEventArgs;

App::App() {
    InitializeComponent();
    fuser::widget::log(L"App created.");
    UnhandledException([](auto const&, UnhandledExceptionEventArgs const& args) {
        fuser::widget::log(L"Unhandled XAML exception " + to_hstring(static_cast<std::int32_t>(args.Exception()))
            + L": " + args.Message());
    });
    Suspending([weak = get_weak()](auto const&, auto const&) {
        if (const auto self = weak.get()) {
            fuser::widget::log(L"App suspending.");
            self->shutdown_current_view();
        }
    });
}

void App::OnLaunched(LaunchActivatedEventArgs const& args) {
    fuser::widget::log(L"Standalone launch.");
    if (args.PrelaunchActivated()) {
        return;
    }
    const auto window = Window::Current();
    if (!window.Content()) {
        const Frame frame;
        frame.Navigate(xaml_typename<SoftwareFuser::MainPage>());
        window.Content(frame);
    }
    window.Activate();
}

void App::OnActivated(IActivatedEventArgs const& args) {
    fuser::widget::log(L"Activation kind=" + to_hstring(static_cast<int>(args.Kind())));
    if (args.Kind() != ActivationKind::Protocol) {
        return;
    }
    const auto protocol = args.try_as<IProtocolActivatedEventArgs>();
    if (!protocol || protocol.Uri().SchemeName() != L"ms-gamebarwidget") {
        return;
    }
    const auto activation = args.try_as<XboxGameBarWidgetActivatedEventArgs>();
    fuser::widget::log(activation ? (activation.IsLaunchActivation() ? L"Widget launch activation." : L"Widget repeat activation.")
                                : L"Activation has no Game Bar arguments.");
    if (!activation || !activation.IsLaunchActivation()) {
        // Keep the initial widget alive during repeat activation.
        return;
    }
    widget_window_ = Window::Current();
    const Frame frame;
    widget_window_.Content(frame);
    widget_ = XboxGameBarWidget{activation, widget_window_.CoreWindow(), frame};
    frame.Navigate(xaml_typename<SoftwareFuser::MainPage>(), widget_);
    closed_token_ = widget_window_.Closed([weak = get_weak()](auto const&, auto const&) {
        if (const auto self = weak.get()) {
            fuser::widget::log(L"Widget window closed.");
            self->shutdown_current_view();
        }
    });
    widget_window_.Activate();
    fuser::widget::log(L"Widget window activation completed.");
}

void App::shutdown_current_view() noexcept {
    try {
        if (widget_window_) {
            if (const auto frame = widget_window_.Content().try_as<Frame>()) {
                if (const auto page = frame.Content().try_as<SoftwareFuser::MainPage>()) {
                    get_self<MainPage>(page)->shutdown();
                }
            }
            widget_window_.Closed(closed_token_);
        }
    } catch (...) {
        // Shutdown must not throw into a Windows lifecycle callback.
        OutputDebugStringW(L"Software Fuser: view cleanup encountered an error.\n");
    }
    widget_ = nullptr;
    widget_window_ = nullptr;
}

} // namespace winrt::SoftwareFuser::implementation
