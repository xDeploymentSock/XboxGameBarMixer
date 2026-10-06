#include <fuser/configuration.h>
#include <fuser/latest_frame_mailbox.h>
#include <fuser/monitor_layout.h>
#include <fuser/widget_layout_requests.h>

#include <cmath>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
class test_surface final : public fuser::gpu_surface {};

void require(bool condition, std::string_view message) {
    if (!condition) {
        throw std::runtime_error{std::string{message}};
    }
}

fuser::decoded_frame frame(std::uint64_t sequence) {
    fuser::decoded_frame result;
    result.surface = std::make_shared<test_surface>();
    result.sequence = sequence;
    return result;
}
}

int main() {
    try {
        fuser::overlay_configuration configuration;
        require(!fuser::validate(configuration).empty(), "streaming requires a host");
        require(fuser::validate(configuration, false).empty(), "offline defaults are valid");
        configuration.host.address = "192.0.2.10";
        require(fuser::validate(configuration).empty(), "1440p240 profile is representable");
        configuration.stream.width = 2559;
        require(!fuser::validate(configuration).empty(), "4:2:0 rejects odd dimensions");
        configuration.stream.width = 2560;
        configuration.stream.frames_per_second = 0;
        require(!fuser::validate(configuration).empty(), "zero FPS is invalid");
        configuration.stream.frames_per_second = 240;
        configuration.key.opacity = std::numeric_limits<float>::quiet_NaN();
        require(!fuser::validate(configuration).empty(), "NaN cannot reach a shader");

        const auto monitor = fuser::monitor_view_extent(2560, 1440, 1.0);
        require(fuser::matches_monitor_extent(2560, 1440, monitor, 1.0), "1440p coverage uses the whole display");
        require(!fuser::matches_monitor_extent(1920, 1080, monitor, 1.0), "a smaller widget cannot count as full coverage");
        const auto scaled_monitor = fuser::monitor_view_extent(2560, 1440, 1.5);
        require(fuser::matches_monitor_extent(1707, 960, scaled_monitor, 1.5), "fractional DPI allows one physical pixel of rounding");
        require(!fuser::matches_monitor_extent(1705, 960, scaled_monitor, 1.5), "fractional DPI must not hide clipped pixels");
        const auto second_monitor = fuser::monitor_view_extent(1920, 1080, 1.25);
        require(fuser::matches_monitor_extent(1536, 864, second_monitor, 1.25), "moving to a different display uses its own size and DPI");
        bool invalid_display_rejected{};
        try { (void)fuser::monitor_view_extent(2560, 1440, 0.0); }
        catch (const std::invalid_argument&) { invalid_display_rejected = true; }
        require(invalid_display_rejected, "invalid display scale must not reach Game Bar");

        require(fuser::matches_monitor_bounds(0, 0, 2560, 1440, monitor, 1.0), "aligned full-monitor bounds match");
        require(!fuser::matches_monitor_bounds(0, -44, 2560, 1440, monitor, 1.0),
                "a full-sized surface shifted above the taskbar is not aligned");
        require(!fuser::matches_monitor_bounds(44, 0, 2560, 1440, monitor, 1.0),
                "a horizontal offset cannot count as aligned coverage");
        require(fuser::matches_monitor_bounds(0.5, -0.5, 1707, 960, scaled_monitor, 1.5),
                "position and size allow one physical pixel of fractional-DPI rounding");
        require(!fuser::matches_monitor_bounds(std::numeric_limits<double>::quiet_NaN(), 0, 2560, 1440, monitor, 1.0),
                "invalid coordinates cannot report aligned bounds");

        fuser::widget_layout_requests layout;
        layout.visibility_changed(true);
        require(!layout.has_pending(), "opening Game Bar does not create an automatic resize");
        layout.request(fuser::widget_layout_action::fit_monitor);
        const auto fit = layout.take();
        require(fit && fit->action == fuser::widget_layout_action::fit_monitor,
                "an explicit fit is ready immediately without waiting for pinning");
        layout.visibility_changed(true);
        require(layout.is_current(*fit) && !layout.has_pending(),
                "foreground/pinned visibility changes do not queue a resize or undo manual placement");
        layout.request(fuser::widget_layout_action::reset_position);
        require(!layout.is_current(*fit), "reset supersedes an in-flight fit before it can apply stale work");
        layout.request(fuser::widget_layout_action::fit_monitor);
        const auto latest_fit = layout.take();
        require(latest_fit && latest_fit->action == fuser::widget_layout_action::fit_monitor,
                "the latest explicit action wins instead of replaying an obsolete reset");
        layout.visibility_changed(false);
        require(!layout.is_current(*latest_fit) && !layout.has_pending(), "hiding cancels in-flight layout work");
        layout.visibility_changed(true);
        require(!layout.has_pending(), "reopening does not recenter the user's manually placed widget");
        layout.request(fuser::widget_layout_action::fit_monitor);
        const auto display_fit = layout.take();
        layout.invalidate();
        require(display_fit && !layout.is_current(*display_fit) && !layout.has_pending(),
                "a display change cancels an obsolete request without moving the widget automatically");

        fuser::latest_frame_mailbox mailbox;
        require(!mailbox.take_latest(), "empty mailbox has no frame");
        auto first = frame(1);
        const std::weak_ptr<const fuser::gpu_surface> old_surface{first.surface};
        require(mailbox.publish(std::move(first)), "first decoded frame is accepted");
        require(mailbox.publish(frame(2)), "newer decoded frame is accepted");
        require(old_surface.expired(), "displaced surface is released");
        require(mailbox.replaced_frames() == 1, "replacement is separately counted");
        const auto latest = mailbox.take_latest();
        require(latest && latest->sequence == 2, "slow renderer consumes only the latest frame");
        require(!mailbox.take_latest(), "consuming removes the pending frame");
        require(!mailbox.publish({}), "null GPU surface is rejected");
        require(mailbox.publish(frame(3)), "frame before shutdown is accepted");
        mailbox.close();
        require(!mailbox.take_latest(), "shutdown releases the pending frame");
        require(!mailbox.publish(frame(4)), "late callbacks cannot refill a closed mailbox");
        mailbox.reset();
        require(mailbox.replaced_frames() == 0, "new session has independent counters");
        require(mailbox.publish(frame(5)), "mailbox can serve a new session");
        std::cout << "Core contract checks passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
