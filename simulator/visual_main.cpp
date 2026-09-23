#include <SDL3/SDL.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>

#include "hyprvx/core/Camera.hpp"
#include "hyprvx/core/CameraMotion.hpp"
#include "hyprvx/core/Island.hpp"
#include "hyprvx/core/World.hpp"

namespace {

using Clock = std::chrono::steady_clock;

constexpr hyprvx::IslandId codeIslandId = 10;

struct InputState {
    bool up{};
    bool down{};
    bool left{};
    bool right{};

    bool middleDragging{};

    Clock::time_point lastDragTime{};
};

enum class DragKind {
    None,
    FreeWindow,
    IslandMember
};

struct WindowDrag {
    DragKind kind{DragKind::None};

    hyprvx::WindowId windowId{};
    hyprvx::IslandId islandId{};

    bool candidate{};
    bool ready{};

    Clock::time_point candidateSince{};

    std::optional<hyprvx::Rect>
        detachReferenceBounds{};

    [[nodiscard]]
    bool active() const noexcept {
        return kind != DragKind::None;
    }

    void clear() noexcept {
        kind = DragKind::None;
        windowId = 0;
        islandId = 0;
        candidate = false;
        ready = false;
        detachReferenceBounds.reset();
    }
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

hyprvx::Rect expanded(
    hyprvx::Rect rect,
    double amount
) {
    rect.position.x -= amount;
    rect.position.y -= amount;

    rect.size.x += amount * 2.0;
    rect.size.y += amount * 2.0;

    return rect;
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

    const double startX =
        std::floor(
            visible.left() / gridSize
        ) * gridSize;

    const double startY =
        std::floor(
            visible.top() / gridSize
        ) * gridSize;

    for (
        double x = startX;
        x <= visible.right();
        x += gridSize
    ) {
        const auto screen =
            camera.worldToScreen({
                x,
                visible.top()
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
        y <= visible.bottom();
        y += gridSize
    ) {
        const auto screen =
            camera.worldToScreen({
                visible.left(),
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

    fillRect(
        renderer,
        {
            static_cast<float>(origin.x),
            0.0F,
            2.0F,
            static_cast<float>(height)
        },
        90,
        100,
        120,
        255
    );

    fillRect(
        renderer,
        {
            0.0F,
            static_cast<float>(origin.y),
            static_cast<float>(width),
            2.0F
        },
        90,
        100,
        120,
        255
    );
}

void drawWindowBody(
    SDL_Renderer* renderer,
    const hyprvx::Rect& worldGeometry,
    const hyprvx::Camera& camera,
    bool selected,
    bool islandMember
) {
    SDL_FRect rect =
        toScreenRect(
            worldGeometry,
            camera
        );

    if (islandMember) {
        fillRect(
            renderer,
            rect,
            40,
            84,
            70,
            255
        );

        outlineRect(
            renderer,
            rect,
            selected ? 245 : 110,
            selected ? 220 : 225,
            selected ? 110 : 175,
            255
        );
    } else {
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
            selected ? 245 : 135,
            selected ? 220 : 175,
            selected ? 110 : 235,
            255
        );
    }

    SDL_FRect title{
        rect.x,
        rect.y,
        rect.w,
        std::max(
            8.0F,
            static_cast<float>(
                38.0 *
                camera.zoom()
            )
        )
    };

    fillRect(
        renderer,
        title,
        islandMember ? 58 : 70,
        islandMember ? 115 : 95,
        islandMember ? 92 : 140,
        255
    );
}

void drawWorld(
    SDL_Renderer* renderer,
    const hyprvx::Camera& camera,
    const hyprvx::World& world,
    const WindowDrag& drag
) {
    const hyprvx::Island* island =
        world.island(codeIslandId);

    if (island != nullptr) {
        const auto bounds =
            island->worldBounds();

        if (bounds.has_value()) {
            hyprvx::Rect padded =
                expanded(
                    *bounds,
                    80.0
                );

            padded.position.y -= 40.0;
            padded.size.y += 40.0;

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

            Uint8 r = 85;
            Uint8 g = 195;
            Uint8 b = 150;

            if (
                drag.kind ==
                DragKind::FreeWindow &&
                drag.candidate
            ) {
                if (drag.ready) {
                    r = 80;
                    g = 240;
                    b = 140;
                } else {
                    r = 245;
                    g = 205;
                    b = 85;
                }
            }

            if (
                drag.kind ==
                DragKind::IslandMember &&
                drag.candidate
            ) {
                r = 245;
                g = 135;
                b = 80;
            }

            outlineRect(
                renderer,
                islandRect,
                r,
                g,
                b,
                255
            );
        }

        for (
            const auto& member :
            island->members()
        ) {
            const auto geometry =
                island->memberWorldGeometry(
                    member.id
                );

            if (!geometry.has_value()) {
                continue;
            }

            drawWindowBody(
                renderer,
                *geometry,
                camera,
                drag.active() &&
                    drag.windowId ==
                    member.id,
                true
            );
        }
    }

    for (
        const auto& [id, window] :
        world.freeWindows()
    ) {
        drawWindowBody(
            renderer,
            window.geometry,
            camera,
            drag.active() &&
                drag.windowId == id,
            false
        );
    }
}

std::optional<
    std::pair<
        hyprvx::IslandId,
        hyprvx::WindowId
    >
>
hitIslandMember(
    const hyprvx::World& world,
    hyprvx::Vec2 worldPoint
) {
    const hyprvx::Island* island =
        world.island(codeIslandId);

    if (island == nullptr) {
        return std::nullopt;
    }

    for (
        auto it =
            island->members().rbegin();

        it !=
            island->members().rend();

        ++it
    ) {
        const auto geometry =
            island->memberWorldGeometry(
                it->id
            );

        if (
            geometry.has_value() &&
            geometry->contains(
                worldPoint
            )
        ) {
            return std::pair{
                island->id(),
                it->id
            };
        }
    }

    return std::nullopt;
}

std::optional<hyprvx::WindowId>
hitFreeWindow(
    const hyprvx::World& world,
    hyprvx::Vec2 worldPoint
) {
    for (
        const auto& [id, window] :
        world.freeWindows()
    ) {
        if (
            window.geometry.contains(
                worldPoint
            )
        ) {
            return id;
        }
    }

    return std::nullopt;
}

void beginLeftDrag(
    hyprvx::World& world,
    const hyprvx::Camera& camera,
    float mouseX,
    float mouseY,
    WindowDrag& drag
) {
    const hyprvx::Vec2 worldPoint =
        camera.screenToWorld({
            static_cast<double>(mouseX),
            static_cast<double>(mouseY)
        });

    if (
        const auto member =
            hitIslandMember(
                world,
                worldPoint
            )
    ) {
        drag.clear();

        drag.kind =
            DragKind::IslandMember;

        drag.islandId =
            member->first;

        drag.windowId =
            member->second;

        if (
            const auto* island =
                world.island(
                    drag.islandId
                )
        ) {
            drag.detachReferenceBounds =
                island->worldBounds();
        }

        return;
    }

    if (
        const auto free =
            hitFreeWindow(
                world,
                worldPoint
            )
    ) {
        drag.clear();

        drag.kind =
            DragKind::FreeWindow;

        drag.windowId =
            *free;
    }
}

void updateFreeJoinCandidate(
    const hyprvx::World& world,
    WindowDrag& drag
) {
    if (
        drag.kind !=
        DragKind::FreeWindow
    ) {
        return;
    }

    const auto* window =
        world.freeWindow(
            drag.windowId
        );

    const auto* island =
        world.island(
            codeIslandId
        );

    if (
        window == nullptr ||
        island == nullptr
    ) {
        drag.candidate = false;
        drag.ready = false;
        return;
    }

    const auto bounds =
        island->worldBounds();

    if (!bounds.has_value()) {
        drag.candidate = false;
        drag.ready = false;
        return;
    }

    const hyprvx::Rect magnetic =
        expanded(
            *bounds,
            220.0
        );

    const bool inside =
        magnetic.contains(
            window->geometry.center()
        );

    if (!inside) {
        drag.candidate = false;
        drag.ready = false;
        return;
    }

    if (!drag.candidate) {
        drag.candidate = true;
        drag.ready = false;

        drag.candidateSince =
            Clock::now();

        return;
    }

    const double dwell =
        std::chrono::duration<double>(
            Clock::now() -
            drag.candidateSince
        ).count();

    if (dwell >= 0.25) {
        drag.ready = true;
    }
}

void updateDetachCandidate(
    const hyprvx::World& world,
    WindowDrag& drag
) {
    if (
        drag.kind !=
            DragKind::IslandMember ||
        !drag.detachReferenceBounds
            .has_value()
    ) {
        return;
    }

    const auto* island =
        world.island(
            drag.islandId
        );

    if (island == nullptr) {
        return;
    }

    const auto geometry =
        island->memberWorldGeometry(
            drag.windowId
        );

    if (!geometry.has_value()) {
        return;
    }

    const hyprvx::Rect keepZone =
        expanded(
            *drag.detachReferenceBounds,
            160.0
        );

    drag.candidate =
        !keepZone.contains(
            geometry->center()
        );

    drag.ready =
        drag.candidate;
}

void moveDraggedWindow(
    hyprvx::World& world,
    const hyprvx::Camera& camera,
    float xrel,
    float yrel,
    WindowDrag& drag
) {
    const hyprvx::Vec2 delta{
        static_cast<double>(xrel) /
            camera.zoom(),

        static_cast<double>(yrel) /
            camera.zoom()
    };

    if (
        drag.kind ==
        DragKind::FreeWindow
    ) {
        if (
            auto* window =
                world.freeWindow(
                    drag.windowId
                )
        ) {
            window->geometry.position +=
                delta;
        }

        updateFreeJoinCandidate(
            world,
            drag
        );

        return;
    }

    if (
        drag.kind ==
        DragKind::IslandMember
    ) {
        auto* island =
            world.island(
                drag.islandId
            );

        if (island == nullptr) {
            return;
        }

        auto* member =
            island->findMember(
                drag.windowId
            );

        if (member == nullptr) {
            return;
        }

        member->localGeometry.position +=
            delta;

        updateDetachCandidate(
            world,
            drag
        );
    }
}

void finishLeftDrag(
    hyprvx::World& world,
    WindowDrag& drag
) {
    if (
        drag.kind ==
            DragKind::FreeWindow &&
        drag.ready
    ) {
        world.joinFreeWindowToIsland(
            drag.windowId,
            codeIslandId
        );
    } else if (
        drag.kind ==
            DragKind::IslandMember &&
        drag.ready
    ) {
        world.detachWindowFromIsland(
            drag.windowId,
            drag.islandId
        );
    }

    drag.clear();
}

void updateWindowTitle(
    SDL_Window* window,
    const hyprvx::Camera& camera,
    const hyprvx::CameraController& controller,
    const WindowDrag& drag
) {
    std::ostringstream stream;

    stream
        << std::fixed
        << std::setprecision(1)
        << "HyprVX Simulator | Camera "
        << camera.position().x
        << ", "
        << camera.position().y
        << " | Zoom "
        << camera.zoom() * 100.0
        << "%";

    if (controller.isFlying()) {
        stream << " | FLYING";
    }

    if (drag.candidate) {
        if (
            drag.kind ==
            DragKind::FreeWindow
        ) {
            stream
                << (
                    drag.ready
                    ? " | RELEASE TO JOIN"
                    : " | JOIN CANDIDATE"
                );
        } else {
            stream
                << " | RELEASE TO DETACH";
        }
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
    const hyprvx::World& world,
    const WindowDrag& drag
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
        world,
        drag
    );

    SDL_RenderPresent(renderer);

    updateWindowTitle(
        window,
        camera,
        controller,
        drag
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
        codeIslandId,
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
    WindowDrag windowDrag;

    bool running = true;
    bool dirty = true;

    auto previousFrame =
        Clock::now();

    render(
        renderer,
        window,
        camera,
        controller,
        world,
        windowDrag
    );

    std::cout
        << "\nHyprVX island simulator controls:\n"
        << "  LMB drag      : move window / island member\n"
        << "  Middle drag   : pan canvas\n"
        << "  WASD / arrows : pan camera\n"
        << "  Wheel         : zoom around cursor\n"
        << "  F             : fly to CODE island\n"
        << "  R / Home      : smooth Go Home\n"
        << "  Esc           : quit\n\n";

    while (running) {
        const bool motionActive =
            keyboardActive(input) ||
            input.middleDragging ||
            windowDrag.active() ||
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
                                current.key.key ==
                                    SDLK_R ||
                                current.key.key ==
                                    SDLK_HOME
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

                        if (
                            current.button.button ==
                            SDL_BUTTON_LEFT
                        ) {
                            controller.cancelFlight();

                            beginLeftDrag(
                                world,
                                camera,
                                current.button.x,
                                current.button.y,
                                windowDrag
                            );

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

                        if (
                            current.button.button ==
                            SDL_BUTTON_LEFT
                        ) {
                            finishLeftDrag(
                                world,
                                windowDrag
                            );

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

                        if (windowDrag.active()) {
                            moveDraggedWindow(
                                world,
                                camera,
                                current.motion.xrel,
                                current.motion.yrel,
                                windowDrag
                            );

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

        if (
            windowDrag.kind ==
            DragKind::FreeWindow
        ) {
            updateFreeJoinCandidate(
                world,
                windowDrag
            );

            dirty = true;
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

        controller.setPanInput(
            keyboardDirection(input)
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
                world,
                windowDrag
            );

            dirty = false;
        }
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
