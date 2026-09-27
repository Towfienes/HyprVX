#pragma once

#include <cstdint>
#include <stdexcept>

namespace hyprvx::platform {

// Pure scheduling and configure state. One dirty frame is queued while a
// Wayland frame callback is outstanding; no callback chain runs when idle.
class OverlayState {
public:
    void configure(std::uint32_t width, std::uint32_t height, int scale) {
        if (width == 0 || height == 0 || width > 16384 || height > 16384 ||
            scale < 1 || scale > 8 ||
            static_cast<std::uint64_t>(width) * scale > 16384 ||
            static_cast<std::uint64_t>(height) * scale > 16384 ||
            static_cast<std::uint64_t>(width) * height * scale * scale > 16'000'000)
            throw std::invalid_argument("invalid overlay dimensions or scale");
        width_ = width;
        height_ = height;
        scale_ = scale;
        dirty_ = true;
    }
    void setInteractive(bool interactive) noexcept {
        if (interactive_ != interactive) { interactive_ = interactive; dirty_ = true; }
    }
    void markDirty() noexcept { dirty_ = true; }
    [[nodiscard]] bool beginFrame() noexcept {
        if (!dirty_ || pending_ || width_ == 0 || height_ == 0) return false;
        dirty_ = false;
        pending_ = true;
        return true;
    }
    void frameDone() noexcept { pending_ = false; }
    void frameFailed() noexcept { pending_ = false; dirty_ = true; }
    [[nodiscard]] bool dirty() const noexcept { return dirty_; }
    [[nodiscard]] bool pending() const noexcept { return pending_; }
    [[nodiscard]] bool interactive() const noexcept { return interactive_; }
    [[nodiscard]] std::uint32_t width() const noexcept { return width_; }
    [[nodiscard]] std::uint32_t height() const noexcept { return height_; }
    [[nodiscard]] int scale() const noexcept { return scale_; }
    [[nodiscard]] std::uint32_t bufferWidth() const noexcept { return width_ * static_cast<std::uint32_t>(scale_); }
    [[nodiscard]] std::uint32_t bufferHeight() const noexcept { return height_ * static_cast<std::uint32_t>(scale_); }
private:
    std::uint32_t width_{};
    std::uint32_t height_{};
    int scale_{1};
    bool interactive_{};
    bool dirty_{true};
    bool pending_{};
};

} // namespace hyprvx::platform
