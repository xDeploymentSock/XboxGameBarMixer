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

        require(fuser::parse_widget_dimension(" 2560 ") == 2560, "typed overlay dimensions allow surrounding whitespace");
        require(fuser::parse_widget_dimension("2559") == 2559, "overlay dimensions are independent of even-sized video negotiation");
        for (const auto invalid : {"", " ", "0", "-1440", "+1440", "1440.5", "1440px", "4294967296"}) {
            bool rejected{};
            try { (void)fuser::parse_widget_dimension(invalid); }
            catch (const std::invalid_argument&) { rejected = true; }
            require(rejected, "malformed or overflowing overlay dimensions must not queue a resize");
        }
        const auto typed = fuser::widget_view_extent(2560, 1440, 1.25);
        require(typed.width == 2048 && typed.height == 1152, "Apply converts physical dimensions to the current display's view pixels");
        for (const auto invalid : {fuser::widget_pixel_extent{239, 1440}, {2560, 239}, {7681, 1440}, {2560, 4321}, {0, 1440}}) {
            bool rejected{};
            try { (void)fuser::widget_view_extent(invalid.width, invalid.height, 1.0); }
            catch (const std::invalid_argument&) { rejected = true; }
            require(rejected, "out-of-range dimensions must not reach Game Bar");
        }
        bool render_limit_rejected{};
        try { (void)fuser::widget_view_extent(18000, 1440, 3.0); }
        catch (const std::invalid_argument&) { render_limit_rejected = true; }
        require(render_limit_rejected, "high DPI cannot bypass the physical render-size limit");

        const auto top_gap = fuser::uncovered_monitor_edges(0, 46, 2558, 1394, monitor, 1.0);
        require(top_gap.left == 0 && top_gap.top == 46 && top_gap.right == 2 && top_gap.bottom == 0,
                "the observed Game Bar title-bar gap is measured outside the render surface");
        const auto above = fuser::uncovered_monitor_edges(0, -44, 2560, 1440, monitor, 1.0);
        require(above.top == 0 && above.bottom == 44, "moving upward alone can expose a bottom gap");
        const auto scaled_gap = fuser::uncovered_monitor_edges(0, 30, 1706, 930, scaled_monitor, 1.5);
        require(scaled_gap.top == 45 && scaled_gap.bottom == 0, "gap values remain physical pixels at fractional DPI");
        bool invalid_gap_rejected{};
        try { (void)fuser::uncovered_monitor_edges(0, std::numeric_limits<double>::quiet_NaN(), 2560, 1440, monitor, 1.0); }
        catch (const std::invalid_argument&) { invalid_gap_rejected = true; }
        require(invalid_gap_rejected, "invalid bounds cannot display misleading zero gaps");

        require(fuser::matches_monitor_bounds(0, 0, 2560, 1440, monitor, 1.0), "aligned full-monitor bounds match");
        require(!fuser::matches_monitor_bounds(0, -44, 2560, 1440, monitor, 1.0),
                "a full-sized surface shifted above the taskbar is not aligned");
        require(!fuser::matches_monitor_bounds(44, 0, 2560, 1440, monitor, 1.0),
                "a horizontal offset cannot count as aligned coverage");
        require(fuser::matches_monitor_bounds(0.5, -0.5, 1707, 960, scaled_monitor, 1.5),
                "position and size allow one physical pixel of fractional-DPI rounding");
        require(!fuser::matches_monitor_bounds(std::numeric_limits<double>::quiet_NaN(), 0, 2560, 1440, monitor, 1.0),
                "invalid coordinates cannot report aligned bounds");

        const auto overscan = fuser::contained_monitor_viewport(0, -44, 2560, 1484, monitor);
        require(overscan && fuser::matches_monitor_bounds(overscan->left, -44 + overscan->top,
                    overscan->width, overscan->height, monitor, 1.0),
                "an extra-height client must contain an aligned full-size inner viewport");
        require(overscan->height == 1440 && -44 + overscan->top + 1439 == 1439,
                "the final source row must retain its original screen coordinate rather than being stretched");
        const auto reopened = fuser::contained_monitor_viewport(0, 0, 2560, 1484, monitor);
        require(reopened && reopened->top == 0 && reopened->height == overscan->height,
                "moving the host to screen zero changes padding, not the video height");
        const auto horizontal_padding = fuser::contained_monitor_viewport(-16, -44, 2576, 1484, monitor);
        require(horizontal_padding && horizontal_padding->left == 16 && horizontal_padding->width == 2560,
                "an off-screen left strip is absorbed without cropping or shrinking the source");
        require(!fuser::contained_monitor_viewport(0, -44, 2560, 1440, monitor)
                    && !fuser::contained_monitor_viewport(0, -88, 2560, 1484, monitor)
                    && !fuser::contained_monitor_viewport(0, 46, 2560, 1440, monitor)
                    && !fuser::contained_monitor_viewport(0, 0, 2558, 1440, monitor),
                "missing bottom, top, or right rows, including the visible overscan offset, cannot be repaired inside the client");
        require(!fuser::contained_monitor_viewport(0, std::numeric_limits<double>::quiet_NaN(), 2560, 1484, monitor)
                    && !fuser::contained_monitor_viewport(0, -44, 2560, std::numeric_limits<double>::infinity(), monitor)
                    && !fuser::contained_monitor_viewport(0, 0, 2560, 1440, {0, 1440}),
                "invalid host or monitor geometry cannot authorize a viewport");

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

        fuser::widget_pixel_extent input{1920, 1080};
        layout.apply_dimensions(input);
        input = {2560, 1440};
        const auto dimensions = layout.take();
        require(dimensions && dimensions->action == fuser::widget_layout_action::apply_dimensions
                    && dimensions->pixels.width == 1920 && dimensions->pixels.height == 1080,
                "an in-flight Apply retains the dimensions captured at the click");
        layout.request(fuser::widget_layout_action::full_screen_fit);
        require(!layout.is_current(*dimensions), "a later full-screen click supersedes custom dimensions");
        layout.apply_dimensions({2560, 1440});
        const auto latest_dimensions = layout.take();
        require(latest_dimensions && latest_dimensions->action == fuser::widget_layout_action::apply_dimensions,
                "custom dimensions supersede a queued full-screen request");
        layout.visibility_changed(false);
        require(!layout.is_current(*latest_dimensions) && !layout.has_pending(), "hiding cancels custom dimensions too");
        layout.visibility_changed(true);
        require(!layout.has_pending(), "saved custom dimensions do not apply automatically on reopening");

        layout.request(fuser::widget_layout_action::pinned_constraints);
        require(!layout.take() && layout.has_pending(),
                "an explicitly armed pinned test waits without resizing in foreground mode");
        layout.visibility_changed(true);
        const auto pinned_test = layout.take(true);
        require(pinned_test && pinned_test->action == fuser::widget_layout_action::pinned_constraints
                    && !layout.has_pending(),
                "the armed test is consumed once after entering visible pinned-only mode");
        require(!layout.take(true), "later host callbacks do not repeat a consumed pinned test");
        layout.request(fuser::widget_layout_action::pinned_constraints);
        layout.request(fuser::widget_layout_action::reset_position);
        const auto recovery = layout.take();
        require(recovery && recovery->action == fuser::widget_layout_action::reset_position
                    && !layout.is_current(*pinned_test),
                "Reset cancels an armed or in-flight pinned test and is ready in foreground mode");
        layout.request(fuser::widget_layout_action::pinned_constraints);
        layout.invalidate();
        require(!layout.take(true), "display changes cancel an armed pinned test");
        layout.request(fuser::widget_layout_action::pinned_constraints);
        layout.pinning_changed(false);
        require(!layout.take(true), "unpinning cancels an armed test even while the widget remains visible");
        layout.request(fuser::widget_layout_action::pinned_constraints);
        layout.visibility_changed(false);
        require(!layout.take(true), "hiding an unpinned widget cancels an armed test");

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
