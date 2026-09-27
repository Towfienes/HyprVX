#include <SDL3/SDL.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>

#include "hyprvx/core/Camera.hpp"
#include "hyprvx/core/CameraMotion.hpp"
#include "hyprvx/core/Island.hpp"
#include "hyprvx/core/SpatialNavigation.hpp"
#include "hyprvx/core/NavigationState.hpp"
#include "hyprvx/core/ViewportFraming.hpp"
#include "hyprvx/core/World.hpp"
#include "hyprvx/runtime/RuntimeCameraController.hpp"
#include "hyprvx/runtime/ProjectionStateStore.hpp"
#include "hyprvx/runtime/SpatialProjection.hpp"
#include "hyprvx/hyprland/RuntimeBridge.hpp"

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

struct ManualHistoryInput {
    bool keyboard{};
    bool middle{};
    bool wheelBurst{};

    Clock::time_point lastWheelEvent{};

    [[nodiscard]]
    bool active() const noexcept {
        return
            keyboard ||
            middle ||
            wheelBurst;
    }
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
    const WindowDrag& drag,
    const std::optional<hyprvx::WindowId>& focusedWindow
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
                (
                    focusedWindow.has_value() &&
                    *focusedWindow == member.id
                ) ||
                (
                    drag.active() &&
                    drag.windowId == member.id
                ),
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
            (
                focusedWindow.has_value() &&
                *focusedWindow == id
            ) ||
            (
                drag.active() &&
                drag.windowId == id
            ),
            false
        );
    }
}

void drawWorkspaceRegions(
    SDL_Renderer* renderer,
    const hyprvx::runtime::SpatialProjector& projector,
    const hyprvx::Camera& camera
) {
    for (const auto& [name, region] : projector.workspaceMap().regions()) {
        static_cast<void>(name);
        outlineRect(renderer, toScreenRect(region.bounds, camera), 75, 105, 145, 220);
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



hyprvx::CameraView currentCameraView(
    const hyprvx::Camera& camera
) {
    return {
        camera.position(),
        camera.zoom()
    };
}

std::optional<hyprvx::ZoneId>
zoneIdFromKey(
    SDL_Keycode key
) {
    switch (key) {
        case SDLK_1:
            return 1;

        case SDLK_2:
            return 2;

        case SDLK_3:
            return 3;

        default:
            return std::nullopt;
    }
}

std::optional<hyprvx::WindowId>
windowAtPoint(
    const hyprvx::World& world,
    hyprvx::Vec2 worldPoint
) {
    if (
        const auto member =
            hitIslandMember(
                world,
                worldPoint
            )
    ) {
        return member->second;
    }

    return hitFreeWindow(
        world,
        worldPoint
    );
}

void updateWindowTitle(
    SDL_Window* window,
    const hyprvx::Camera& camera,
    const hyprvx::CameraController& controller,
    const WindowDrag& drag,
    const std::optional<hyprvx::WindowId>& focusedWindow
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

    if (focusedWindow.has_value()) {
        stream
            << " | FOCUS "
            << *focusedWindow;
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
    const hyprvx::runtime::SpatialProjector* projector,
    const WindowDrag& drag,
    const std::optional<hyprvx::WindowId>& focusedWindow
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

    if (projector != nullptr) drawWorkspaceRegions(renderer, *projector, camera);

    drawWorld(
        renderer,
        camera,
        world,
        drag,
        focusedWindow
    );

    SDL_RenderPresent(renderer);

    updateWindowTitle(
        window,
        camera,
        controller,
        drag,
        focusedWindow
    );
}

}

int main(int argc, char** argv) {
    using namespace hyprvx;

    std::string capturePath;
    std::string statePath;
    for (int index = 1; index < argc; ++index) {
        const std::string argument(argv[index]);
        if (argument == "--capture" && index + 1 < argc) capturePath = argv[++index];
        else if (argument == "--state" && index + 1 < argc) statePath = argv[++index];
        else if (argument == "--help") {
            std::cout << "Usage: hyprvx-visual-sim [--capture <socket2.log>] [--state <state.json>]\n";
            return 0;
        } else {
            std::cerr << "Unknown or incomplete option: " << argument << "\n";
            return 2;
        }
    }

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

    hyprvx::hyprland::RuntimeBridge runtimeBridge;
    hyprvx::runtime::SpatialProjector projector;
    projector.setCompositorEpoch("visual-simulator");
    if (!capturePath.empty()) {
        if (!statePath.empty()) {
            const auto loaded = hyprvx::runtime::ProjectionStateStore{}.load(statePath);
            if (loaded.state) projector.restoreState(*loaded.state, world);
            else if (loaded.status != hyprvx::runtime::StateLoadStatus::Missing) {
                std::cerr << "Ignoring saved state: " << loaded.diagnostic << "\n";
            }
        }
        std::ifstream capture(capturePath, std::ios::binary);
        if (!capture) {
            std::cerr << "Could not open capture: " << capturePath << "\n";
            SDL_DestroyRenderer(renderer);
            SDL_DestroyWindow(window);
            SDL_Quit();
            return 1;
        }
        std::string line;
        while (std::getline(capture, line)) {
            static_cast<void>(runtimeBridge.processLine(line));
            projector.reconcile(runtimeBridge.mirror(), world);
        }
        if (capture.bad()) {
            std::cerr << "Failed reading capture: " << capturePath << "\n";
            SDL_DestroyRenderer(renderer);
            SDL_DestroyWindow(window);
            SDL_Quit();
            return 1;
        }
    }

    if (capturePath.empty()) world.addFreeWindow({
        1,
        {
            {350.0, 250.0},
            {900.0, 600.0}
        }
    });

    if (capturePath.empty()) world.addFreeWindow({
        4,
        {
            {-1800.0, 1000.0},
            {700.0, 500.0}
        }
    });

    if (capturePath.empty()) {
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
    }

    int initialWidth{};
    int initialHeight{};
    SDL_GetWindowSizeInPixels(window, &initialWidth, &initialHeight);
    hyprvx::runtime::RuntimeCameraController navigation{
        camera, controller, world,
        {static_cast<double>(initialWidth), static_cast<double>(initialHeight)}};
    if (!capturePath.empty()) {
        navigation.observeCompositorSelection(projector.focusedWindowId());
    }

    InputState input;
    WindowDrag windowDrag;

    ManualHistoryInput
        manualHistoryInput;

    ZoneStore zones;

    zones.set({
        1,
        "Home",
        {
            {0.0, 0.0},
            1.0
        }
    });

    zones.set({
        2,
        "Code",
        {
            {2500.0, -1450.0},
            1.0
        }
    });

    zones.set({
        3,
        "Notes",
        {
            {-2200.0, 750.0},
            1.0
        }
    });

    std::optional<WindowId>
        focusedWindow{};
    if (!capturePath.empty()) focusedWindow = navigation.selectedWindow();

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
        capturePath.empty() ? nullptr : &projector,
        windowDrag,
        focusedWindow
    );

    std::cout
        << "\nHyprVX spatial simulator controls:\n"
        << "  LMB drag      : move window / island member\n"
        << "  Middle drag   : pan canvas\n"
        << "  H/J/K/L, WASD : pan camera\n"
        << "  arrows        : pan camera\n"
        << "  Wheel         : zoom around cursor\n"
        << "  F             : fly to CODE island\n"
        << "  Shift+H/J/K/L : spatial focus navigation\n"
        << "  Alt+Left/Right: camera history back/forward\n"
        << "  1 / 2 / 3     : workspace region in capture mode, otherwise Zone\n"
        << "  Ctrl+1/2/3    : save current view to Zone\n"
        << "  R / Home      : smooth Go Home\n"
        << "  Esc           : quit\n\n";
    if (!capturePath.empty()) {
        std::cout << "Replay projection view loaded: " << capturePath << "\n";
    }

    while (running) {
        const bool motionActive =
            keyboardActive(input) ||
            windowDrag.active() ||
            manualHistoryInput.wheelBurst ||
            navigation.needsFrame();

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

                        const bool keyboardWasActive =
                            keyboardActive(input);

                        const bool modifiedArrow =
                            (
                                current.key.mod &
                                (
                                    SDL_KMOD_SHIFT |
                                    SDL_KMOD_ALT |
                                    SDL_KMOD_CTRL
                                )
                            ) != 0;

                        switch (current.key.key) {
                            case SDLK_W:
                            case SDLK_K:
                                if ((current.key.mod & SDL_KMOD_SHIFT) != 0 && current.key.key == SDLK_K) break;
                                input.up = pressed;
                                break;

                            case SDLK_S:
                            case SDLK_J:
                                if ((current.key.mod & SDL_KMOD_SHIFT) != 0 && current.key.key == SDLK_J) break;
                                input.down = pressed;
                                break;

                            case SDLK_A:
                            case SDLK_H:
                                if ((current.key.mod & SDL_KMOD_SHIFT) != 0 && current.key.key == SDLK_H) break;
                                input.left = pressed;
                                break;

                            case SDLK_D:
                            case SDLK_L:
                                if ((current.key.mod & SDL_KMOD_SHIFT) != 0 && current.key.key == SDLK_L) break;
                                input.right = pressed;
                                break;

                            case SDLK_UP:
                                if (
                                    !modifiedArrow ||
                                    !pressed
                                ) {
                                    input.up = pressed;
                                }
                                break;

                            case SDLK_DOWN:
                                if (
                                    !modifiedArrow ||
                                    !pressed
                                ) {
                                    input.down = pressed;
                                }
                                break;

                            case SDLK_LEFT:
                                if (
                                    !modifiedArrow ||
                                    !pressed
                                ) {
                                    input.left = pressed;
                                }
                                break;

                            case SDLK_RIGHT:
                                if (
                                    !modifiedArrow ||
                                    !pressed
                                ) {
                                    input.right = pressed;
                                }
                                break;

                            default:
                                break;
                        }

                        const bool keyboardNowActive =
                            keyboardActive(input);

                        if (
                            !keyboardWasActive &&
                            keyboardNowActive
                        ) {
                            manualHistoryInput.keyboard =
                                true;
                        }

                        if (
                            keyboardWasActive &&
                            !keyboardNowActive
                        ) {
                            manualHistoryInput.keyboard =
                                false;
                        }

                        if (
                            current.type ==
                                SDL_EVENT_KEY_DOWN &&
                            !current.key.repeat
                        ) {
                            const bool shiftHeld =
                                (
                                    current.key.mod &
                                    SDL_KMOD_SHIFT
                                ) != 0;

                            const bool altHeld =
                                (
                                    current.key.mod &
                                    SDL_KMOD_ALT
                                ) != 0;

                            const bool ctrlHeld =
                                (
                                    current.key.mod &
                                    SDL_KMOD_CTRL
                                ) != 0;

                            std::optional<
                                SpatialDirection
                            > spatialDirection{};

                            if (shiftHeld) {
                                switch (current.key.key) {
                                    case SDLK_H:
                                        spatialDirection =
                                            SpatialDirection::Left;
                                        break;

                                    case SDLK_L:
                                        spatialDirection =
                                            SpatialDirection::Right;
                                        break;

                                    case SDLK_K:
                                        spatialDirection =
                                            SpatialDirection::Up;
                                        break;

                                    case SDLK_J:
                                        spatialDirection =
                                            SpatialDirection::Down;
                                        break;

                                    default:
                                        break;
                                }
                            }

                            if (
                                spatialDirection.has_value() &&
                                focusedWindow.has_value()
                            ) {
                                navigation.observeCompositorSelection(focusedWindow);
                                if (navigation.focusDirection(*spatialDirection)) {
                                    std::cout
                                        << "Spatial focus: "
                                        << *focusedWindow
                                        << " -> "
                                        << *navigation.selectedWindow()
                                        << "\n";
                                    focusedWindow = navigation.selectedWindow();

                                    dirty = true;
                                } else {
                                    std::cout
                                        << "Spatial focus: no candidate from "
                                        << *focusedWindow
                                        << "\n";
                                }
                            }

                            if (
                                altHeld &&
                                current.key.key ==
                                    SDLK_LEFT
                            ) {
                                if (navigation.historyBack()) {

                                    std::cout
                                        << "History: back"
                                        << std::endl;

                                    dirty = true;
                                } else {
                                    std::cout
                                        << "History: no back entry"
                                        << std::endl;
                                }
                            }

                            if (
                                altHeld &&
                                current.key.key ==
                                    SDLK_RIGHT
                            ) {
                                if (navigation.historyForward()) {

                                    std::cout
                                        << "History: forward"
                                        << std::endl;

                                    dirty = true;
                                } else {
                                    std::cout
                                        << "History: no forward entry"
                                        << std::endl;
                                }
                            }

                            const auto zoneId =
                                zoneIdFromKey(
                                    current.key.key
                                );

                            if (
                                zoneId.has_value() &&
                                ctrlHeld
                            ) {
                                const Zone* existing =
                                    zones.find(
                                        *zoneId
                                    );

                                std::string name =
                                    existing != nullptr
                                    ? existing->name
                                    : (
                                        "Zone " +
                                        std::to_string(
                                            *zoneId
                                        )
                                    );

                                zones.set({
                                    *zoneId,
                                    std::move(name),
                                    currentCameraView(
                                        camera
                                    )
                                });

                                std::cout
                                    << "Zone "
                                    << *zoneId
                                    << " saved"
                                    << std::endl;
                            } else if (
                                zoneId.has_value() &&
                                !altHeld &&
                                !shiftHeld
                            ) {
                                const std::string workspaceName = std::to_string(*zoneId);
                                if (!capturePath.empty() && projector.workspaceMap().find(workspaceName) != nullptr) {
                                    static_cast<void>(navigation.gotoWorkspace(workspaceName));
                                    std::cout << "HyprVX workspace region " << workspaceName << std::endl;
                                    dirty = true;
                                } else if (const Zone* zone = zones.find(*zoneId); zone != nullptr) {
                                    static_cast<void>(navigation.gotoView(zone->view));

                                    std::cout
                                        << "Zone "
                                        << zone->id
                                        << ": "
                                        << zone->name
                                        << std::endl;

                                    dirty = true;
                                }
                            }

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
                                const Zone* home =
                                    zones.find(1);

                                navigation.observeCompositorSelection(focusedWindow);
                                if (focusedWindow) static_cast<void>(navigation.home());
                                else if (home != nullptr) static_cast<void>(navigation.gotoView(home->view));

                                dirty = true;
                            }

                            if (
                                current.key.key ==
                                SDLK_F
                            ) {
                                const Zone* code =
                                    zones.find(2);

                                if (code != nullptr) {
                                    static_cast<void>(navigation.gotoView(code->view));
                                }

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
                            static_cast<void>(navigation.panBegin());

                            manualHistoryInput.middle =
                                true;

                            input.lastDragTime =
                                Clock::now();

                            dirty = true;
                        }

                        if (
                            current.button.button ==
                            SDL_BUTTON_LEFT
                        ) {
                            controller.cancelFlight();

                            const Vec2 clickWorld =
                                camera.screenToWorld({
                                    static_cast<double>(
                                        current.button.x
                                    ),
                                    static_cast<double>(
                                        current.button.y
                                    )
                                });

                            if (
                                const auto clicked =
                                    windowAtPoint(
                                        world,
                                        clickWorld
                                    )
                            ) {
                                focusedWindow = *clicked;
                                navigation.observeCompositorSelection(focusedWindow);

                                std::cout
                                    << "Focused window: "
                                    << *focusedWindow
                                    << std::endl;
                            }

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

                            manualHistoryInput.middle =
                                false;

                            if (
                                !manualHistoryInput.keyboard &&
                                !manualHistoryInput.wheelBurst
                            ) {
                                if (navigation.panEnd()) {
                                    std::cout
                                        << "Manual history: MMB recorded"
                                        << std::endl;
                                }
                            }

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

                            static_cast<void>(dt);
                            navigation.panByScreen({
                                static_cast<double>(current.motion.xrel),
                                static_cast<double>(current.motion.yrel)});

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
                        if (
                            !manualHistoryInput.wheelBurst
                        ) {
                            manualHistoryInput.wheelBurst =
                                true;
                            static_cast<void>(navigation.panBegin());
                        }

                        manualHistoryInput.lastWheelEvent =
                            Clock::now();

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
                        SDL_GetWindowSizeInPixels(window, &initialWidth, &initialHeight);
                        navigation.setViewport({static_cast<double>(initialWidth), static_cast<double>(initialHeight)});
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

        if (
            manualHistoryInput.wheelBurst
        ) {
            const double wheelIdleSeconds =
                std::chrono::duration<double>(
                    now -
                    manualHistoryInput
                        .lastWheelEvent
                ).count();

            if (
                wheelIdleSeconds >=
                0.18
            ) {
                manualHistoryInput.wheelBurst =
                    false;

                if (
                    !manualHistoryInput.keyboard &&
                    !manualHistoryInput.middle
                ) {
                    if (navigation.panEnd()) {
                        std::cout
                            << "Manual history: wheel recorded"
                            << std::endl;
                    }
                }
            }
        }

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

        navigation.setPanInput(
            keyboardDirection(input)
        );

        if (!input.middleDragging) {
            const bool shouldUpdate =
                navigation.needsFrame();

            if (shouldUpdate) {
                static_cast<void>(navigation.tick(deltaSeconds));

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
                capturePath.empty() ? nullptr : &projector,
                windowDrag,
                focusedWindow
            );

            dirty = false;
        }
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
