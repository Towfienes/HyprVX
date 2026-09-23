#include <SDL3/SDL.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>

#include "hyprvx/core/Camera.hpp"
#include "hyprvx/core/CameraMotion.hpp"
#include "hyprvx/core/Island.hpp"
#include "hyprvx/core/World.hpp"

namespace {

using Clock = std::chrono::steady_clock;

struct InputState {
    bool up{};
    bool down{};
    bool left{};
    bool right{};

    bool middleDragging{};

    Clock::time_point lastDragTime{};
};

hyprvx::Vec2 keyboardDirection(
    const InputState& input
) {
    hyprvx::Vec2 direction{};

    if (input.left) {
        direction.x -= 1.0;
    }

    if (input.right) {
        direction.x += 1.0;
    }

    if (input.up) {
        direction.y -= 1.0;
    }

    if (input.down) {
        direction.y += 1.0;
    }

    return direction;
}

bool keyboardActive(
    const InputState& input
) {
    return
        input.up ||
        input.down ||
        input.left ||
        input.right;
}

SDL_FRect toScreenRect(
    const hyprvx::Rect& worldRect,
    const hyprvx::Camera& camera
) {
    const hyprvx::Vec2 topLeft =
        camera.worldToScreen(
            worldRect.position
        );

    return SDL_FRect{
        static_cast<float>(topLeft.x),
        static_cast<float>(topLeft.y),
        static_cast<float>(
            worldRect.size.x *
            camera.zoom()
        ),
        static_cast<float>(
            worldRect.size.y *
            camera.zoom()
        )
    };
}

void fillRect(
    SDL_Renderer* renderer,
    const SDL_FRect& rect,
    Uint8 r,
    Uint8 g,
    Uint8 b,
    Uint8 a
) {
    SDL_SetRenderDrawColor(
        renderer,
        r,
        g,
        b,
        a
    );

    SDL_RenderFillRect(
        renderer,
        &rect
    );
}

void outlineRect(
    SDL_Renderer* renderer,
    const SDL_FRect& rect,
    Uint8 r,
    Uint8 g,
    Uint8 b,
    Uint8 a
) {
    SDL_SetRenderDrawColor(
        renderer,
        r,
        g,
        b,
        a
    );

    SDL_RenderRect(
        renderer,
        &rect
    );
}

void drawGrid(
    SDL_Renderer* renderer,
    const hyprvx::Camera& camera,
    int width,
    int height
) {
    constexpr double gridSize = 250.0;

    const hyprvx::Rect visible =
        camera.visibleWorldRect({
            static_cast<double>(width),
            static_cast<double>(height)
        });

    const double left =
        visible.left();

    const double right =
        visible.right();

    const double top =
        visible.top();

    const double bottom =
        visible.bottom();

    const double startX =
        std::floor(left / gridSize) *
        gridSize;

    const double startY =
        std::floor(top / gridSize) *
        gridSize;

    for (
        double x = startX;
        x <= right;
        x += gridSize
    ) {
        const auto screen =
            camera.worldToScreen({
                x,
                top
            });

        SDL_FRect line{
            static_cast<float>(screen.x),
            0.0F,
            1.0F,
            static_cast<float>(height)
        };

        fillRect(
            renderer,
            line,
            38,
            43,
            52,
            255
        );
    }

    for (
        double y = startY;
        y <= bottom;
        y += gridSize
    ) {
        const auto screen =
            camera.worldToScreen({
                left,
                y
            });

        SDL_FRect line{
            0.0F,
            static_cast<float>(screen.y),
            static_cast<float>(width),
            1.0F
        };

        fillRect(
            renderer,
            line,
            38,
            43,
            52,
            255
        );
    }

    const auto origin =
        camera.worldToScreen({
            0.0,
            0.0
        });

    SDL_FRect vertical{
        static_cast<float>(origin.x),
        0.0F,
        2.0F,
        static_cast<float>(height)
    };

    SDL_FRect horizontal{
        0.0F,
        static_cast<float>(origin.y),
        static_cast<float>(width),
        2.0F
    };

    fillRect(
        renderer,
        vertical,
        90,
        100,
        120,
        255
    );

    fillRect(
        renderer,
        horizontal,
        90,
        100,
        120,
        255
    );
}

void drawWorld(
    SDL_Renderer* renderer,
    const hyprvx::Camera& camera,
    const hyprvx::World& world
) {
    if (
        const auto* browser =
            world.freeWindow(1)
    ) {
        const SDL_FRect rect =
            toScreenRect(
                browser->geometry,
                camera
            );

        fillRect(
            renderer,
            rect,
            55,
            75,
            110,
            255
        );

        outlineRect(
            renderer,
            rect,
            135,
            175,
            235,
            255
        );
    }

    if (
        const auto* notes =
            world.freeWindow(4)
    ) {
        const SDL_FRect rect =
            toScreenRect(
                notes->geometry,
                camera
            );

        fillRect(
            renderer,
            rect,
            75,
            65,
            95,
            255
        );

        outlineRect(
            renderer,
            rect,
            175,
            145,
            225,
            255
        );
    }

    const hyprvx::Island* island =
        world.island(10);

    if (island == nullptr) {
        return;
    }

    const auto bounds =
        island->worldBounds();

    if (bounds.has_value()) {
        hyprvx::Rect padded =
            *bounds;

        padded.position.x -= 60.0;
        padded.position.y -= 100.0;

        padded.size.x += 120.0;
        padded.size.y += 160.0;

        const SDL_FRect islandRect =
            toScreenRect(
                padded,
                camera
            );

        fillRect(
            renderer,
            islandRect,
            31,
            47,
            43,
            255
        );

        outlineRect(
            renderer,
            islandRect,
            85,
            195,
            150,
            255
        );
    }

    if (
        const auto member =
            island->memberWorldGeometry(2)
    ) {
        const SDL_FRect rect =
            toScreenRect(
                *member,
                camera
            );

        fillRect(
            renderer,
            rect,
            38,
            82,
            69,
            255
        );

        outlineRect(
            renderer,
            rect,
            110,
            225,
            175,
            255
        );
    }

    if (
        const auto member =
            island->memberWorldGeometry(3)
    ) {
        const SDL_FRect rect =
            toScreenRect(
                *member,
                camera
            );

        fillRect(
            renderer,
            rect,
            42,
            68,
            59,
            255
        );

        outlineRect(
            renderer,
            rect,
            95,
            205,
            160,
            255
        );
    }
}

void updateWindowTitle(
    SDL_Window* window,
    const hyprvx::Camera& camera,
    const hyprvx::CameraController& controller
) {
    std::ostringstream stream;

    stream
        << std::fixed
        << std::setprecision(1)
        << "HyprVX Simulator  |  Camera "
        << camera.position().x
        << ", "
        << camera.position().y
        << "  |  Zoom "
        << camera.zoom() * 100.0
        << "%";

    if (controller.isFlying()) {
        stream << "  |  FLYING";
    }

    SDL_SetWindowTitle(
        window,
        stream.str().c_str()
    );
}

void render(
    SDL_Renderer* renderer,
    SDL_Window* window,
    const hyprvx::Camera& camera,
    const hyprvx::CameraController& controller,
    const hyprvx::World& world
) {
    int width{};
    int height{};

    SDL_GetWindowSizeInPixels(
        window,
        &width,
        &height
    );

    SDL_SetRenderDrawColor(
        renderer,
        18,
        21,
        27,
        255
    );

    SDL_RenderClear(renderer);

    drawGrid(
        renderer,
        camera,
        width,
        height
    );

    drawWorld(
        renderer,
        camera,
        world
    );

    SDL_RenderPresent(renderer);

    updateWindowTitle(
        window,
        camera,
        controller
    );
}

}

int main() {
    using namespace hyprvx;

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::cerr
            << "SDL_Init failed: "
            << SDL_GetError()
            << "\n";

        return 1;
    }

    SDL_Window* window =
        SDL_CreateWindow(
            "HyprVX Simulator",
            1400,
            900,
            SDL_WINDOW_RESIZABLE
        );

    if (window == nullptr) {
        std::cerr
            << "SDL_CreateWindow failed: "
            << SDL_GetError()
            << "\n";

        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer =
        SDL_CreateRenderer(
            window,
            nullptr
        );

    if (renderer == nullptr) {
        std::cerr
            << "SDL_CreateRenderer failed: "
            << SDL_GetError()
            << "\n";

        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    Camera camera{
        {0.0, 0.0},
        1.0
    };

    CameraMotionConfig motionConfig;

    motionConfig.acceleration = 8500.0;
    motionConfig.damping = 7.5;
    motionConfig.maxSpeed = 2200.0;
    motionConfig.flightResponse = 5.5;

    motionConfig.minZoom = 0.20;
    motionConfig.maxZoom = 3.00;

    CameraController controller{
        camera,
        motionConfig
    };

    World world;

    world.addFreeWindow({
        1,
        {
            {350.0, 250.0},
            {900.0, 600.0}
        }
    });

    world.addFreeWindow({
        4,
        {
            {-1800.0, 1000.0},
            {700.0, 500.0}
        }
    });

    Island code{
        10,
        {2600.0, -1300.0}
    };

    code.addMember({
        2,
        {
            {0.0, 0.0},
            {1000.0, 700.0}
        }
    });

    code.addMember({
        3,
        {
            {1060.0, 80.0},
            {760.0, 620.0}
        }
    });

    world.addIsland(
        std::move(code)
    );

    InputState input;

    bool running = true;
    bool dirty = true;

    auto previousFrame =
        Clock::now();

    render(
        renderer,
        window,
        camera,
        controller,
        world
    );

    std::cout
        << "\nHyprVX visual simulator controls:\n"
        << "  WASD / arrows : pan camera\n"
        << "  Middle drag   : direct canvas pan\n"
        << "  Wheel         : zoom around cursor\n"
        << "  F             : fly to CODE island\n"
        << "  R / Home      : smooth Go Home\n"
        << "  Esc           : quit\n\n";

    while (running) {
        const bool motionActive =
            keyboardActive(input) ||
            input.middleDragging ||
            controller.isFlying() ||
            controller.velocity()
                .lengthSquared() > 0.01;

        SDL_Event event{};

        bool receivedEvent{};

        if (motionActive) {
            receivedEvent =
                SDL_WaitEventTimeout(
                    &event,
                    8
                );
        } else {
            receivedEvent =
                SDL_WaitEvent(
                    &event
                );
        }

        auto processEvent =
            [&](const SDL_Event& current) {
                switch (current.type) {
                    case SDL_EVENT_QUIT:
                        running = false;
                        break;

                    case SDL_EVENT_KEY_DOWN:
                    case SDL_EVENT_KEY_UP: {
                        const bool pressed =
                            current.type ==
                            SDL_EVENT_KEY_DOWN;

                        switch (current.key.key) {
                            case SDLK_W:
                            case SDLK_UP:
                                input.up = pressed;
                                break;

                            case SDLK_S:
                            case SDLK_DOWN:
                                input.down = pressed;
                                break;

                            case SDLK_A:
                            case SDLK_LEFT:
                                input.left = pressed;
                                break;

                            case SDLK_D:
                            case SDLK_RIGHT:
                                input.right = pressed;
                                break;

                            default:
                                break;
                        }

                        if (
                            current.type ==
                            SDL_EVENT_KEY_DOWN &&
                            !current.key.repeat
                        ) {
                            if (
                                current.key.key ==
                                SDLK_ESCAPE
                            ) {
                                running = false;
                            }

                            if (
                                current.key.key == SDLK_R ||
                                current.key.key == SDLK_HOME
                            ) {
                                controller.flyToView(
                                    {0.0, 0.0},
                                    1.0
                                );

                                dirty = true;
                            }

                            if (
                                current.key.key ==
                                SDLK_F
                            ) {
                                controller.flyTo({
                                    2500.0,
                                    -1450.0
                                });

                                dirty = true;
                            }
                        }

                        dirty = true;
                        break;
                    }

                    case SDL_EVENT_MOUSE_BUTTON_DOWN:
                        if (
                            current.button.button ==
                            SDL_BUTTON_MIDDLE
                        ) {
                            input.middleDragging = true;

                            controller.cancelFlight();

                            input.lastDragTime =
                                Clock::now();

                            dirty = true;
                        }
                        break;

                    case SDL_EVENT_MOUSE_BUTTON_UP:
                        if (
                            current.button.button ==
                            SDL_BUTTON_MIDDLE
                        ) {
                            input.middleDragging = false;
                            dirty = true;
                        }
                        break;

                    case SDL_EVENT_MOUSE_MOTION:
                        if (input.middleDragging) {
                            const auto now =
                                Clock::now();

                            const double dt =
                                std::chrono::duration<double>(
                                    now -
                                    input.lastDragTime
                                ).count();

                            input.lastDragTime = now;

                            const Vec2 delta{
                                -static_cast<double>(
                                    current.motion.xrel
                                ) / camera.zoom(),

                                -static_cast<double>(
                                    current.motion.yrel
                                ) / camera.zoom()
                            };

                            camera.moveBy(delta);

                            if (dt > 0.000001) {
                                controller.setVelocity(
                                    delta / dt
                                );
                            }

                            dirty = true;
                        }
                        break;

                    case SDL_EVENT_MOUSE_WHEEL: {
                        float mouseX{};
                        float mouseY{};

                        SDL_GetMouseState(
                            &mouseX,
                            &mouseY
                        );

                        const double factor =
                            std::exp(
                                static_cast<double>(
                                    current.wheel.y
                                ) * 0.14
                            );

                        controller
                            .zoomByFactorAroundScreenPoint(
                                factor,
                                {
                                    static_cast<double>(
                                        mouseX
                                    ),
                                    static_cast<double>(
                                        mouseY
                                    )
                                }
                            );

                        dirty = true;
                        break;
                    }

                    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
                    case SDL_EVENT_WINDOW_RESIZED:
                    case SDL_EVENT_WINDOW_EXPOSED:
                        dirty = true;
                        break;

                    default:
                        break;
                }
            };

        if (receivedEvent) {
            processEvent(event);

            while (
                SDL_PollEvent(&event)
            ) {
                processEvent(event);
            }
        }

        if (!running) {
            break;
        }

        const auto now =
            Clock::now();

        double deltaSeconds =
            std::chrono::duration<double>(
                now -
                previousFrame
            ).count();

        previousFrame = now;

        deltaSeconds =
            std::clamp(
                deltaSeconds,
                0.0,
                0.05
            );

        const Vec2 direction =
            keyboardDirection(input);

        controller.setPanInput(
            direction
        );

        if (!input.middleDragging) {
            const bool shouldUpdate =
                keyboardActive(input) ||
                controller.isFlying() ||
                controller.velocity()
                    .lengthSquared() > 0.01;

            if (shouldUpdate) {
                controller.update(
                    deltaSeconds
                );

                dirty = true;
            }
        }

        if (dirty) {
            render(
                renderer,
                window,
                camera,
                controller,
                world
            );

            dirty = false;
        }
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
