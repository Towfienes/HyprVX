#include <SDL3/SDL.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <optional>
#include <string>

#include "hyprvx/core/Camera.hpp"
#include "hyprvx/core/Island.hpp"
#include "hyprvx/core/OutputState.hpp"
#include "hyprvx/core/OutputTopology.hpp"
#include "hyprvx/core/World.hpp"

namespace {

using Clock = std::chrono::steady_clock;

constexpr const char* laptopName = "eDP-2";
constexpr const char* externalName = "DP-1";

struct Pane {
    float x{};
    float y{};
    float width{};
    float height{};
};

struct KeyState {
    bool up{};
    bool down{};
    bool left{};
    bool right{};

    [[nodiscard]]
    bool active() const noexcept {
        return up || down || left || right;
    }
};

hyprvx::OutputDescriptor laptopDescriptor() {
    return {
        laptopName,
        {0.0, 0.0},
        {2880.0, 1800.0},
        1.0,
        120.0
    };
}

hyprvx::OutputDescriptor externalDescriptor() {
    return {
        externalName,
        {2880.0, 0.0},
        {2560.0, 1440.0},
        1.0,
        210.0
    };
}

hyprvx::Vec2 keyDirection(
    const KeyState& keys
) {
    hyprvx::Vec2 value{};

    if (keys.left) {
        value.x -= 1.0;
    }

    if (keys.right) {
        value.x += 1.0;
    }

    if (keys.up) {
        value.y -= 1.0;
    }

    if (keys.down) {
        value.y += 1.0;
    }

    const double length = value.length();

    if (length > 1.0) {
        value = value / length;
    }

    return value;
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

SDL_FRect toPaneRect(
    const hyprvx::Rect& worldRect,
    const hyprvx::Camera& camera,
    const Pane& pane
) {
    const hyprvx::Vec2 screen =
        camera.worldToScreen(
            worldRect.position
        );

    return {
        pane.x +
            static_cast<float>(screen.x),

        pane.y +
            static_cast<float>(screen.y),

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

void drawGrid(
    SDL_Renderer* renderer,
    const hyprvx::Camera& camera,
    const Pane& pane
) {
    constexpr double grid = 250.0;

    const hyprvx::Rect visible =
        camera.visibleWorldRect({
            static_cast<double>(
                pane.width
            ),
            static_cast<double>(
                pane.height
            )
        });

    const double startX =
        std::floor(
            visible.left() / grid
        ) * grid;

    const double startY =
        std::floor(
            visible.top() / grid
        ) * grid;

    SDL_SetRenderDrawColor(
        renderer,
        38,
        43,
        52,
        255
    );

    for (
        double x = startX;
        x <= visible.right();
        x += grid
    ) {
        const auto point =
            camera.worldToScreen({
                x,
                visible.top()
            });

        SDL_RenderLine(
            renderer,
            pane.x +
                static_cast<float>(
                    point.x
                ),
            pane.y,
            pane.x +
                static_cast<float>(
                    point.x
                ),
            pane.y + pane.height
        );
    }

    for (
        double y = startY;
        y <= visible.bottom();
        y += grid
    ) {
        const auto point =
            camera.worldToScreen({
                visible.left(),
                y
            });

        SDL_RenderLine(
            renderer,
            pane.x,
            pane.y +
                static_cast<float>(
                    point.y
                ),
            pane.x + pane.width,
            pane.y +
                static_cast<float>(
                    point.y
                )
        );
    }
}

void drawWorld(
    SDL_Renderer* renderer,
    const hyprvx::World& world,
    const hyprvx::Camera& camera,
    const Pane& pane
) {
    for (
        const auto& [id, window] :
        world.freeWindows()
    ) {
        static_cast<void>(id);

        const SDL_FRect rect =
            toPaneRect(
                window.geometry,
                camera,
                pane
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

    for (
        const auto& [islandId, island] :
        world.islands()
    ) {
        static_cast<void>(islandId);

        const auto bounds =
            island.worldBounds();

        if (bounds.has_value()) {
            hyprvx::Rect padded =
                *bounds;

            padded.position.x -= 60.0;
            padded.position.y -= 80.0;

            padded.size.x += 120.0;
            padded.size.y += 140.0;

            const SDL_FRect islandRect =
                toPaneRect(
                    padded,
                    camera,
                    pane
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

        for (
            const auto& member :
            island.members()
        ) {
            const auto geometry =
                island.memberWorldGeometry(
                    member.id
                );

            if (!geometry.has_value()) {
                continue;
            }

            const SDL_FRect rect =
                toPaneRect(
                    *geometry,
                    camera,
                    pane
                );

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
                110,
                225,
                175,
                255
            );
        }
    }
}

void renderPane(
    SDL_Renderer* renderer,
    const hyprvx::World& world,
    const hyprvx::MultiOutputState& outputs,
    const std::string& outputName,
    const std::optional<std::string>& primary,
    bool physicallyConnected,
    const Pane& pane
) {
    SDL_FRect background{
        pane.x,
        pane.y,
        pane.width,
        pane.height
    };

    const hyprvx::OutputState* output =
        outputs.find(outputName);

    if (output == nullptr) {
        fillRect(
            renderer,
            background,
            physicallyConnected ? 16 : 8,
            physicallyConnected ? 17 : 9,
            physicallyConnected ? 22 : 11,
            255
        );

        outlineRect(
            renderer,
            background,
            physicallyConnected ? 100 : 50,
            physicallyConnected ? 100 : 50,
            physicallyConnected ? 110 : 55,
            255
        );

        return;
    }

    fillRect(
        renderer,
        background,
        18,
        21,
        27,
        255
    );

    SDL_Rect clip{
        static_cast<int>(pane.x),
        static_cast<int>(pane.y),
        static_cast<int>(pane.width),
        static_cast<int>(pane.height)
    };

    SDL_SetRenderClipRect(
        renderer,
        &clip
    );

    drawGrid(
        renderer,
        output->camera,
        pane
    );

    drawWorld(
        renderer,
        world,
        output->camera,
        pane
    );

    SDL_SetRenderClipRect(
        renderer,
        nullptr
    );

    const bool isPrimary =
        primary.has_value() &&
        *primary == outputName;

    outlineRect(
        renderer,
        background,
        isPrimary ? 235 : 80,
        isPrimary ? 210 : 90,
        isPrimary ? 100 : 110,
        255
    );
}

hyprvx::OutputTopologySnapshot
makeSnapshot(
    const hyprvx::MultiOutputState& outputs,
    bool laptopConnected,
    bool externalConnected,
    const std::optional<std::string>& primary
) {
    hyprvx::OutputTopologySnapshot snapshot;

    snapshot.outputs[laptopName] = {
        laptopConnected,
        outputs.isActive(
            laptopName
        )
    };

    snapshot.outputs[externalName] = {
        externalConnected,
        outputs.isActive(
            externalName
        )
    };

    snapshot.primary = primary;

    return snapshot;
}

void printCamera(
    const char* prefix,
    const std::string& outputName,
    const hyprvx::Camera* camera
) {
    std::cout
        << prefix
        << " "
        << outputName;

    if (camera == nullptr) {
        std::cout
            << " camera unavailable"
            << std::endl;

        return;
    }

    std::cout
        << " camera "
        << camera->position().x
        << ", "
        << camera->position().y
        << " zoom "
        << camera->zoom()
        << std::endl;
}

void reconcile(
    hyprvx::MultiOutputState& outputs,
    const hyprvx::OutputTopologyPlanner& planner,
    bool laptopConnected,
    bool externalConnected,
    std::optional<std::string>& primary,
    std::string& selected
) {
    const auto snapshot =
        makeSnapshot(
            outputs,
            laptopConnected,
            externalConnected,
            primary
        );

    const auto actions =
        planner.plan(snapshot);

    if (actions.empty()) {
        std::cout
            << "Topology: already settled"
            << std::endl;

        return;
    }

    for (
        const auto& action :
        actions
    ) {
        switch (action.kind) {
            case hyprvx::OutputTopologyActionKind::
                Activate: {

                std::cout
                    << "Topology action: activate "
                    << action.output
                    << std::endl;

                if (
                    action.output ==
                    laptopName
                ) {
                    outputs.activate(
                        laptopDescriptor(),
                        hyprvx::Camera{
                            {0.0, 0.0},
                            0.65
                        }
                    );
                } else if (
                    action.output ==
                    externalName
                ) {
                    outputs.activate(
                        externalDescriptor(),
                        hyprvx::Camera{
                            {2100.0, -1600.0},
                            0.65
                        }
                    );
                }

                printCamera(
                    "Activated",
                    action.output,
                    outputs.camera(
                        action.output
                    )
                );

                break;
            }

            case hyprvx::OutputTopologyActionKind::
                SelectPrimary:

                std::cout
                    << "Topology action: primary "
                    << action.output
                    << std::endl;

                primary =
                    action.output;

                selected =
                    action.output;

                break;

            case hyprvx::OutputTopologyActionKind::
                Deactivate:

                printCamera(
                    "Storing",
                    action.output,
                    outputs.camera(
                        action.output
                    )
                );

                std::cout
                    << "Topology action: deactivate "
                    << action.output
                    << std::endl;

                outputs.deactivate(
                    action.output
                );

                break;
        }
    }

    const auto settled =
        planner.plan(
            makeSnapshot(
                outputs,
                laptopConnected,
                externalConnected,
                primary
            )
        );

    if (!settled.empty()) {
        std::cerr
            << "WARNING: topology did not settle in one plan"
            << std::endl;
    }
}

void updateTitle(
    SDL_Window* window,
    bool laptopConnected,
    bool externalConnected,
    const hyprvx::MultiOutputState& outputs,
    const std::optional<std::string>& primary
) {
    std::string title =
        "HyprVX Topology";

    title +=
        " | eDP phys=" +
        std::string(
            laptopConnected
            ? "on"
            : "off"
        );

    title +=
        " active=" +
        std::string(
            outputs.isActive(laptopName)
            ? "on"
            : "off"
        );

    title +=
        " | DP phys=" +
        std::string(
            externalConnected
            ? "on"
            : "off"
        );

    title +=
        " active=" +
        std::string(
            outputs.isActive(externalName)
            ? "on"
            : "off"
        );

    title +=
        " | primary=" +
        (
            primary.has_value()
            ? *primary
            : std::string("none")
        );

    SDL_SetWindowTitle(
        window,
        title.c_str()
    );
}

}

int main() {
    using namespace hyprvx;

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::cerr
            << SDL_GetError()
            << std::endl;

        return 1;
    }

    SDL_Window* window =
        SDL_CreateWindow(
            "HyprVX Topology",
            1600,
            900,
            SDL_WINDOW_RESIZABLE
        );

    if (window == nullptr) {
        std::cerr
            << SDL_GetError()
            << std::endl;

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
            << SDL_GetError()
            << std::endl;

        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

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

    MultiOutputState outputs;
    OutputTopologyPlanner planner;

    bool laptopConnected = true;
    bool externalConnected = false;

    std::optional<std::string>
        primary{};

    std::string selected =
        laptopName;

    reconcile(
        outputs,
        planner,
        laptopConnected,
        externalConnected,
        primary,
        selected
    );

    KeyState keys;

    bool running = true;
    bool dirty = true;

    auto previous =
        Clock::now();

    std::cout
        << "\nHyprVX topology simulator:\n"
        << "  P             : plug/unplug DP-1\n"
        << "  E             : connect/disconnect eDP-2 hardware\n"
        << "  WASD / arrows : pan primary camera\n"
        << "  Wheel         : zoom primary camera\n"
        << "  Esc           : quit\n\n";

    while (running) {
        SDL_Event event{};

        bool received{};

        if (keys.active()) {
            received =
                SDL_WaitEventTimeout(
                    &event,
                    8
                );
        } else {
            received =
                SDL_WaitEvent(
                    &event
                );
        }

        auto process =
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

                        switch (
                            current.key.key
                        ) {
                            case SDLK_W:
                            case SDLK_UP:
                                keys.up = pressed;
                                break;

                            case SDLK_S:
                            case SDLK_DOWN:
                                keys.down = pressed;
                                break;

                            case SDLK_A:
                            case SDLK_LEFT:
                                keys.left = pressed;
                                break;

                            case SDLK_D:
                            case SDLK_RIGHT:
                                keys.right = pressed;
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
                                SDLK_P
                            ) {
                                externalConnected =
                                    !externalConnected;

                                std::cout
                                    << "\nPhysical DP-1: "
                                    << (
                                        externalConnected
                                        ? "CONNECTED"
                                        : "DISCONNECTED"
                                    )
                                    << std::endl;

                                reconcile(
                                    outputs,
                                    planner,
                                    laptopConnected,
                                    externalConnected,
                                    primary,
                                    selected
                                );

                                dirty = true;
                            }

                            if (
                                current.key.key ==
                                SDLK_E
                            ) {
                                laptopConnected =
                                    !laptopConnected;

                                std::cout
                                    << "\nPhysical eDP-2: "
                                    << (
                                        laptopConnected
                                        ? "CONNECTED"
                                        : "DISCONNECTED"
                                    )
                                    << std::endl;

                                reconcile(
                                    outputs,
                                    planner,
                                    laptopConnected,
                                    externalConnected,
                                    primary,
                                    selected
                                );

                                dirty = true;
                            }
                        }

                        break;
                    }

                    case SDL_EVENT_MOUSE_WHEEL: {
                        if (
                            !primary.has_value()
                        ) {
                            break;
                        }

                        Camera* camera =
                            outputs.camera(
                                *primary
                            );

                        if (camera == nullptr) {
                            break;
                        }

                        float mouseX{};
                        float mouseY{};

                        SDL_GetMouseState(
                            &mouseX,
                            &mouseY
                        );

                        int width{};
                        int height{};

                        SDL_GetWindowSizeInPixels(
                            window,
                            &width,
                            &height
                        );

                        const bool laptopPrimary =
                            *primary ==
                            laptopName;

                        const float paneOffset =
                            laptopPrimary
                            ? 0.0F
                            : static_cast<float>(
                                width
                            ) / 2.0F;

                        const double factor =
                            std::exp(
                                static_cast<double>(
                                    current.wheel.y
                                ) * 0.14
                            );

                        camera
                            ->setZoomAroundScreenPoint(
                                std::clamp(
                                    camera->zoom() *
                                        factor,
                                    0.20,
                                    3.00
                                ),
                                {
                                    static_cast<double>(
                                        mouseX -
                                        paneOffset
                                    ),
                                    static_cast<double>(
                                        mouseY
                                    )
                                }
                            );

                        dirty = true;
                        break;
                    }

                    case SDL_EVENT_WINDOW_RESIZED:
                    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
                    case SDL_EVENT_WINDOW_EXPOSED:
                        dirty = true;
                        break;

                    default:
                        break;
                }
            };

        if (received) {
            process(event);

            while (
                SDL_PollEvent(
                    &event
                )
            ) {
                process(event);
            }
        }

        if (!running) {
            break;
        }

        const auto now =
            Clock::now();

        double dt =
            std::chrono::duration<double>(
                now - previous
            ).count();

        previous = now;

        dt =
            std::clamp(
                dt,
                0.0,
                0.05
            );

        if (
            keys.active() &&
            primary.has_value()
        ) {
            Camera* camera =
                outputs.camera(
                    *primary
                );

            if (camera != nullptr) {
                constexpr double speed =
                    1600.0;

                camera->moveBy(
                    keyDirection(keys) *
                    speed *
                    dt /
                    camera->zoom()
                );

                dirty = true;
            }
        }

        if (dirty) {
            int width{};
            int height{};

            SDL_GetWindowSizeInPixels(
                window,
                &width,
                &height
            );

            const float half =
                static_cast<float>(
                    width
                ) / 2.0F;

            const Pane left{
                0.0F,
                0.0F,
                half,
                static_cast<float>(
                    height
                )
            };

            const Pane right{
                half,
                0.0F,
                static_cast<float>(
                    width
                ) - half,
                static_cast<float>(
                    height
                )
            };

            SDL_SetRenderDrawColor(
                renderer,
                8,
                10,
                13,
                255
            );

            SDL_RenderClear(renderer);

            renderPane(
                renderer,
                world,
                outputs,
                laptopName,
                primary,
                laptopConnected,
                left
            );

            renderPane(
                renderer,
                world,
                outputs,
                externalName,
                primary,
                externalConnected,
                right
            );

            SDL_RenderPresent(
                renderer
            );

            updateTitle(
                window,
                laptopConnected,
                externalConnected,
                outputs,
                primary
            );

            dirty = false;
        }
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
