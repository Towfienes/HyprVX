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
#include "hyprvx/runtime/RuntimeNavigationInput.hpp"
#include "hyprvx/runtime/ProjectionStateStore.hpp"
#include "hyprvx/runtime/SpatialProjection.hpp"
#include "hyprvx/hyprland/RuntimeBridge.hpp"
#include "SdlNavigationInput.hpp"

namespace {

using Clock = std::chrono::steady_clock;
using hyprvx::runtime::RuntimeNavigationAction;
using hyprvx::runtime::RuntimeNavigationActionType;

constexpr hyprvx::IslandId codeIslandId = 10;

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

    WindowDrag windowDrag;
    hyprvx::simulator::SdlNavigationInputAdapter physicalInput;
    hyprvx::runtime::RuntimeNavigationActionDispatcher actionDispatcher(navigation);

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
    Clock::time_point lastWheelEvent{};

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
            physicalInput.keyboardActive() ||
            windowDrag.active() ||
            physicalInput.wheelActive() ||
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

                    case SDL_EVENT_WINDOW_FOCUS_LOST:
                        physicalInput.reset();
                        actionDispatcher.cancelInput();
                        finishLeftDrag(world, windowDrag);
                        lastWheelEvent = {};
                        dirty = true;
                        break;

                    case SDL_EVENT_KEY_DOWN:
                    case SDL_EVENT_KEY_UP: {
                        RuntimeNavigationAction semantic;
                        const bool pressed = current.type == SDL_EVENT_KEY_DOWN;
                        if (physicalInput.keyEvent(current.key.key, pressed, current.key.mod,
                                                   current.key.repeat, semantic)) {
                            const auto type = semantic.type;
                            if (type == RuntimeNavigationActionType::Quit && pressed && !current.key.repeat) {
                                running = false;
                            } else if (pressed && !current.key.repeat && type == RuntimeNavigationActionType::SaveViewSlot) {
                                const Zone* existing = zones.find(static_cast<std::uint64_t>(semantic.slot));
                                zones.set({static_cast<std::uint64_t>(semantic.slot), existing ? existing->name : ("Zone " + std::to_string(semantic.slot)), currentCameraView(camera)});
                            } else if (pressed && !current.key.repeat && type == RuntimeNavigationActionType::GotoViewSlot) {
                                if (const Zone* zone = zones.find(static_cast<std::uint64_t>(semantic.slot)); zone != nullptr) {
                                    RuntimeNavigationAction view;
                                    view.type = RuntimeNavigationActionType::GotoView;
                                    view.view = zone->view;
                                    static_cast<void>(actionDispatcher.submit(view));
                                }
                            } else if (pressed && !current.key.repeat && type == RuntimeNavigationActionType::GotoWorkspace) {
                                const auto zoneId = static_cast<std::uint64_t>(semantic.slot);
                                if (!actionDispatcher.submit(semantic)) {
                                    if (const Zone* zone = zones.find(zoneId); zone != nullptr) {
                                        RuntimeNavigationAction view;
                                        view.type = RuntimeNavigationActionType::GotoView;
                                        view.view = zone->view;
                                        static_cast<void>(actionDispatcher.submit(view));
                                    }
                                }
                            } else {
                                static_cast<void>(actionDispatcher.submit(semantic));
                            }
                            if (type == RuntimeNavigationActionType::FocusDirection || type == RuntimeNavigationActionType::SelectWindow) {
                                focusedWindow = navigation.selectedWindow();
                            }
                        }
                        dirty = true;
                        break;
                    }

                    case SDL_EVENT_MOUSE_BUTTON_DOWN:
                        {
                            RuntimeNavigationAction semantic;
                            if (physicalInput.mouseButtonEvent(current.button.button, true, semantic)) {
                                static_cast<void>(actionDispatcher.submit(semantic));
                                dirty = true;
                            }
                        }

                        if (
                            current.button.button ==
                            SDL_BUTTON_LEFT
                        ) {
                            const Vec2 clickWorld =
                                camera.screenToWorld({
                                    static_cast<double>(
                                        current.button.x
                                    ),
                                    static_cast<double>(
                                        current.button.y
                                    )
                                });

                            const auto clicked = windowAtPoint(world, clickWorld);
                            RuntimeNavigationAction semantic;
                            if (physicalInput.clickEvent(clicked, semantic)) {
                                static_cast<void>(actionDispatcher.submit(semantic));
                                focusedWindow = navigation.selectedWindow();

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
                        {
                            RuntimeNavigationAction semantic;
                            if (physicalInput.mouseButtonEvent(current.button.button, false, semantic)) {
                                static_cast<void>(actionDispatcher.submit(semantic));
                                dirty = true;
                            }
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
                        {
                            RuntimeNavigationAction semantic;
                            if (physicalInput.mouseMotionEvent({static_cast<double>(current.motion.xrel), static_cast<double>(current.motion.yrel)}, semantic)) {
                                static_cast<void>(actionDispatcher.submit(semantic));
                            }
                        }
                        if (physicalInput.middleDragging()) {
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
                        RuntimeNavigationAction semantic;
                        if (physicalInput.wheelBegin(semantic)) static_cast<void>(actionDispatcher.submit(semantic));
                        lastWheelEvent = Clock::now();

                        float mouseX{};
                        float mouseY{};

                        SDL_GetMouseState(
                            &mouseX,
                            &mouseY
                        );

                        if (physicalInput.wheelEvent(static_cast<double>(current.wheel.y),
                                {static_cast<double>(mouseX), static_cast<double>(mouseY)}, semantic)) {
                            static_cast<void>(actionDispatcher.submit(semantic));
                        }

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

        if (physicalInput.wheelActive() && std::chrono::duration<double>(now - lastWheelEvent).count() >= 0.18) {
            RuntimeNavigationAction semantic;
            if (physicalInput.wheelEnd(semantic)) static_cast<void>(actionDispatcher.submit(semantic));
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

        if (actionDispatcher.flushPendingPan()) dirty = true;
        if (navigation.needsFrame()) {
            static_cast<void>(navigation.tick(deltaSeconds));
            dirty = true;
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
