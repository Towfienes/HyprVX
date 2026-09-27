#pragma once

#include "hyprvx/runtime/RuntimeNavigationInput.hpp"

#include <SDL3/SDL.h>

namespace hyprvx::simulator {

// SDL event decoding only. The adapters return device-independent semantic
// actions; camera policy stays in RuntimeNavigationActionDispatcher.
class SdlNavigationInputAdapter {
public:
    [[nodiscard]] bool keyEvent(SDL_Keycode key, bool pressed, SDL_Keymod modifiers,
                                bool repeat, runtime::RuntimeNavigationAction& output);
    [[nodiscard]] bool mouseButtonEvent(Uint8 button, bool pressed,
                                        runtime::RuntimeNavigationAction& output);
    [[nodiscard]] bool mouseMotionEvent(hyprvx::Vec2 delta, runtime::RuntimeNavigationAction& output) const;
    [[nodiscard]] bool clickEvent(std::optional<hyprvx::WindowId> projectedWindow,
                                  runtime::RuntimeNavigationAction& output) const;
    [[nodiscard]] bool wheelBegin(runtime::RuntimeNavigationAction& output);
    [[nodiscard]] bool wheelEvent(double delta, hyprvx::Vec2 anchor, runtime::RuntimeNavigationAction& output) const;
    [[nodiscard]] bool wheelEnd(runtime::RuntimeNavigationAction& output);
    void reset() noexcept;
    [[nodiscard]] bool keyboardActive() const noexcept { return keyboard_.panActive(); }
    [[nodiscard]] bool middleDragging() const noexcept { return mouse_.dragging(); }
    [[nodiscard]] bool wheelActive() const noexcept { return mouse_.wheelActive(); }

private:
    runtime::KeyboardNavigationAdapter keyboard_;
    runtime::MouseNavigationAdapter mouse_;
};

} // namespace hyprvx::simulator
