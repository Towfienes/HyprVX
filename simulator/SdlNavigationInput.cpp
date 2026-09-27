#include "SdlNavigationInput.hpp"

namespace hyprvx::simulator {
namespace {
std::optional<runtime::KeyboardKey> translate(SDL_Keycode key) {
    using K = runtime::KeyboardKey;
    switch (key) {
    case SDLK_H: case SDLK_A: return K::H;
    case SDLK_J: case SDLK_S: return K::J;
    case SDLK_K: case SDLK_W: return K::K;
    case SDLK_L: case SDLK_D: return K::L;
    case SDLK_UP: return K::Up;
    case SDLK_DOWN: return K::Down;
    case SDLK_LEFT: return K::Left;
    case SDLK_RIGHT: return K::Right;
    case SDLK_HOME: return K::Home;
    case SDLK_R: return K::R;
    case SDLK_1: return K::Digit1;
    case SDLK_2: return K::Digit2;
    case SDLK_3: return K::Digit3;
    case SDLK_F: return K::F;
    case SDLK_ESCAPE: return K::Escape;
    default: return std::nullopt;
    }
}
}

bool SdlNavigationInputAdapter::keyEvent(SDL_Keycode key, bool pressed, SDL_Keymod modifiers,
                                         bool repeat, runtime::RuntimeNavigationAction& output) {
    const auto translated = translate(key);
    if (!translated) return false;
    return keyboard_.key(*translated, pressed,
        {(modifiers & SDL_KMOD_SHIFT) != 0, (modifiers & SDL_KMOD_ALT) != 0,
         (modifiers & SDL_KMOD_CTRL) != 0}, repeat, output);
}

bool SdlNavigationInputAdapter::mouseButtonEvent(Uint8 button, bool pressed,
                                                 runtime::RuntimeNavigationAction& output) {
    const auto translated = button == SDL_BUTTON_MIDDLE ? runtime::MouseButton::Middle :
        button == SDL_BUTTON_LEFT ? runtime::MouseButton::Left : runtime::MouseButton::Other;
    return mouse_.button(translated, pressed, output);
}

bool SdlNavigationInputAdapter::mouseMotionEvent(hyprvx::Vec2 delta, runtime::RuntimeNavigationAction& output) const {
    return mouse_.motion(delta, output);
}

bool SdlNavigationInputAdapter::clickEvent(std::optional<hyprvx::WindowId> id,
                                           runtime::RuntimeNavigationAction& output) const {
    return mouse_.click(id, output);
}

bool SdlNavigationInputAdapter::wheelBegin(runtime::RuntimeNavigationAction& output) { return mouse_.wheelBegin(output); }
bool SdlNavigationInputAdapter::wheelEvent(double delta, hyprvx::Vec2 anchor,
                                           runtime::RuntimeNavigationAction& output) const {
    return mouse_.wheel(delta, anchor, output);
}
bool SdlNavigationInputAdapter::wheelEnd(runtime::RuntimeNavigationAction& output) { return mouse_.wheelEnd(output); }

void SdlNavigationInputAdapter::reset() noexcept { keyboard_.reset(); mouse_.reset(); }

} // namespace hyprvx::simulator
