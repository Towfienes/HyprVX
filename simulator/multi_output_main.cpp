#include <SDL3/SDL.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <string>

#include "hyprvx/core/Camera.hpp"
#include "hyprvx/core/Island.hpp"
#include "hyprvx/core/OutputState.hpp"
#include "hyprvx/core/World.hpp"

namespace {

using Clock = std::chrono::steady_clock;

constexpr const char* laptopName =
    "eDP-2";

constexpr const char* externalName =
    "DP-1";

struct KeyState {
    bool up{};
    bool down{};
    bool left{};
    bool right{};

    [[nodiscard]]
    bool active() const noexcept {
        return
            up ||
            down ||
            left ||
            right;
    }
};

struct Pane {
    float x{};
    float y{};
    float width{};
    float height{};
};

hyprvx::OutputDescriptor
laptopDescriptor() {
    return {
        laptopName,
        {0.0, 0.0},
        {2880.0, 1800.0},
        1.0,
        120.0
    };
}

hyprvx::OutputDescriptor
externalDescriptor() {
    return {
        externalName,
        {2880.0, 0.0},
        {2560.0, 1440.0},
        1.0,
        210.0
    };
}

hyprvx::Vec2 direction(
    const KeyState& keys
) {
    hyprvx::Vec2 result{};

    if (keys.left) {
        result.x -= 1.0;
    }

    if (keys.right) {
        result.x += 1.0;
    }

    if (keys.up) {
        result.y -= 1.0;
    }

    if (keys.down) {
        result.y += 1.0;
    }

    const double length =
        result.length();

    if (length > 1.0) {
        result = result / length;
    }

    return result;
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
            static_cast<float>(
                screen.x
            ),

        pane.y +
            static_cast<float>(
                screen.y
            ),

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

void drawPaneBackground(
    SDL_Renderer* renderer,
    const Pane& pane,
    bool activeOutput,
    bool selected
) {
    SDL_FRect rect{
        pane.x,
        pane.y,
        pane.width,
        pane.height
    };

    if (!activeOutput) {
        fillRect(
            renderer,
            rect,
            10,
            11,
            14,
            255
        );

        outlineRect(
            renderer,
            rect,
            65,
            65,
            70,
            255
        );

        return;
    }

    fillRect(
        renderer,
        rect,
        18,
        21,
        27,
        255
    );

    if (selected) {
        outlineRect(
            renderer,
            rect,
            235,
            210,
            100,
            255
        );
    } else {
        outlineRect(
            renderer,
            rect,
            75,
            85,
            100,
            255
        );
    }
}

void drawGrid(
    SDL_Renderer* renderer,
    const hyprvx::Camera& camera,
    const Pane& pane
) {
    constexpr double gridSize =
        250.0;

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
            visible.left() /
            gridSize
        ) * gridSize;

    const double startY =
        std::floor(
            visible.top() /
            gridSize
        ) * gridSize;

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
        x += gridSize
    ) {
        const auto screen =
            camera.worldToScreen({
                x,
                visible.top()
            });

        SDL_RenderLine(
            renderer,
            pane.x +
                static_cast<float>(
                    screen.x
                ),
            pane.y,
            pane.x +
                static_cast<float>(
                    screen.x
                ),
            pane.y +
                pane.height
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

        SDL_RenderLine(
            renderer,
            pane.x,
            pane.y +
                static_cast<float>(
                    screen.y
                ),
            pane.x +
                pane.width,
            pane.y +
                static_cast<float>(
                    screen.y
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

        SDL_FRect rect =
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
        static_cast<void>(
            islandId
        );

        const auto bounds =
            island.worldBounds();

        if (bounds.has_value()) {
            hyprvx::Rect padded =
                *bounds;

            padded.position.x -=
                60.0;

            padded.position.y -=
                80.0;

            padded.size.x +=
                120.0;

            padded.size.y +=
                140.0;

            SDL_FRect islandRect =
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

            SDL_FRect rect =
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

void renderOutput(
    SDL_Renderer* renderer,
    const hyprvx::World& world,
    const hyprvx::MultiOutputState& outputs,
    const std::string& outputName,
    const std::string& selectedOutput,
    const Pane& pane
) {
    const hyprvx::OutputState* output =
        outputs.find(
            outputName
        );

    drawPaneBackground(
        renderer,
        pane,
        output != nullptr,
        outputName ==
            selectedOutput
    );

    if (output == nullptr) {
        return;
    }

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

    SDL_FRect border{
        pane.x,
        pane.y,
        pane.width,
        pane.height
    };

    if (
        outputName ==
        selectedOutput
    ) {
        outlineRect(
            renderer,
            border,
            235,
            210,
            100,
            255
        );
    } else {
        outlineRect(
            renderer,
            border,
            75,
            85,
            100,
            255
        );
    }
}

void updateTitle(
    SDL_Window* window,
    const hyprvx::MultiOutputState& outputs,
    const std::string& selectedOutput
) {
    std::string title =
        "HyprVX Multi-output | Active: " +
        selectedOutput;

    title +=
        outputs.isActive(externalName)
        ? " | DP-1 connected"
        : " | DP-1 disconnected";

    title +=
        outputs.isActive(laptopName)
        ? " | eDP-2 connected"
        : " | eDP-2 disconnected";

    SDL_SetWindowTitle(
        window,
        title.c_str()
    );
}

bool paneContains(
    const Pane& pane,
    float x,
    float y
) {
    return
        x >= pane.x &&
        x < pane.x + pane.width &&
        y >= pane.y &&
        y < pane.y + pane.height;
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
            "HyprVX Multi-output",
            1600,
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

    outputs.activate(
        laptopDescriptor(),
        Camera{
            {0.0, 0.0},
            0.65
        }
    );

    outputs.activate(
        externalDescriptor(),
        Camera{
            {2100.0, -1600.0},
            0.65
        }
    );

    std::string selectedOutput =
        laptopName;

    KeyState keys;

    bool running = true;
    bool dirty = true;

    auto previous =
        Clock::now();

    std::cout
        << "\nHyprVX multi-output simulator:\n"
        << "  1 / 2         : select eDP-2 / DP-1\n"
        << "  Tab           : switch active output\n"
        << "  WASD / arrows : pan selected camera\n"
        << "  Wheel         : zoom selected camera\n"
        << "  R             : reset selected camera\n"
        << "  E             : toggle eDP-2 hotplug\n"
        << "  P             : toggle DP-1 hotplug\n"
        << "  Esc           : quit\n\n";

    while (running) {
        SDL_Event event{};

        bool gotEvent{};

        if (keys.active()) {
            gotEvent =
                SDL_WaitEventTimeout(
                    &event,
                    8
                );
        } else {
            gotEvent =
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

                        switch (
                            current.key.key
                        ) {
                            case SDLK_W:
                            case SDLK_UP:
                                keys.up =
                                    pressed;
                                break;

                            case SDLK_S:
                            case SDLK_DOWN:
                                keys.down =
                                    pressed;
                                break;

                            case SDLK_A:
                            case SDLK_LEFT:
                                keys.left =
                                    pressed;
                                break;

                            case SDLK_D:
                            case SDLK_RIGHT:
                                keys.right =
                                    pressed;
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
                                SDLK_1 &&
                                outputs.isActive(
                                    laptopName
                                )
                            ) {
                                selectedOutput =
                                    laptopName;

                                dirty = true;
                            }

                            if (
                                current.key.key ==
                                SDLK_2 &&
                                outputs.isActive(
                                    externalName
                                )
                            ) {
                                selectedOutput =
                                    externalName;

                                dirty = true;
                            }

                            if (
                                current.key.key ==
                                SDLK_TAB
                            ) {
                                if (
                                    selectedOutput ==
                                        laptopName &&
                                    outputs.isActive(
                                        externalName
                                    )
                                ) {
                                    selectedOutput =
                                        externalName;
                                } else if (
                                    outputs.isActive(
                                        laptopName
                                    )
                                ) {
                                    selectedOutput =
                                        laptopName;
                                }

                                dirty = true;
                            }

                            if (
                                current.key.key ==
                                SDLK_R
                            ) {
                                Camera* camera =
                                    outputs.camera(
                                        selectedOutput
                                    );

                                if (
                                    camera != nullptr
                                ) {
                                    camera->setPosition({
                                        0.0,
                                        0.0
                                    });

                                    camera->setZoom(
                                        0.65
                                    );

                                    dirty = true;
                                }
                            }

                            if (
                                current.key.key ==
                                SDLK_P
                            ) {
                                if (
                                    outputs.isActive(
                                        externalName
                                    )
                                ) {
                                    const Camera* camera =
                                        outputs.camera(
                                            externalName
                                        );

                                    if (
                                        camera != nullptr
                                    ) {
                                        std::cout
                                            << "DP-1 unplug at camera "
                                            << camera->position().x
                                            << ", "
                                            << camera->position().y
                                            << " zoom "
                                            << camera->zoom()
                                            << "\n";
                                    }

                                    outputs.deactivate(
                                        externalName
                                    );

                                    if (
                                        selectedOutput ==
                                        externalName
                                    ) {
                                        selectedOutput =
                                            laptopName;
                                    }
                                } else {
                                    outputs.activate(
                                        externalDescriptor(),
                                        Camera{
                                            {0.0, 0.0},
                                            0.65
                                        }
                                    );

                                    const Camera* camera =
                                        outputs.camera(
                                            externalName
                                        );

                                    if (
                                        camera != nullptr
                                    ) {
                                        std::cout
                                            << "DP-1 replug restored camera "
                                            << camera->position().x
                                            << ", "
                                            << camera->position().y
                                            << " zoom "
                                            << camera->zoom()
                                            << "\n";
                                    }
                                }

                                dirty = true;
                            }

                            if (
                                current.key.key ==
                                SDLK_E
                            ) {
                                if (
                                    outputs.isActive(
                                        laptopName
                                    )
                                ) {
                                    outputs.deactivate(
                                        laptopName
                                    );

                                    if (
                                        selectedOutput ==
                                            laptopName &&
                                        outputs.isActive(
                                            externalName
                                        )
                                    ) {
                                        selectedOutput =
                                            externalName;
                                    }
                                } else {
                                    outputs.activate(
                                        laptopDescriptor(),
                                        Camera{
                                            {0.0, 0.0},
                                            0.65
                                        }
                                    );

                                    selectedOutput =
                                        laptopName;
                                }

                                dirty = true;
                            }
                        }

                        break;
                    }

                    case SDL_EVENT_MOUSE_BUTTON_DOWN: {
                        int width{};
                        int height{};

                        SDL_GetWindowSizeInPixels(
                            window,
                            &width,
                            &height
                        );

                        const Pane left{
                            0.0F,
                            0.0F,
                            static_cast<float>(
                                width
                            ) / 2.0F,
                            static_cast<float>(
                                height
                            )
                        };

                        const Pane right{
                            left.width,
                            0.0F,
                            static_cast<float>(
                                width
                            ) -
                                left.width,
                            static_cast<float>(
                                height
                            )
                        };

                        if (
                            paneContains(
                                left,
                                current.button.x,
                                current.button.y
                            ) &&
                            outputs.isActive(
                                laptopName
                            )
                        ) {
                            selectedOutput =
                                laptopName;
                        }

                        if (
                            paneContains(
                                right,
                                current.button.x,
                                current.button.y
                            ) &&
                            outputs.isActive(
                                externalName
                            )
                        ) {
                            selectedOutput =
                                externalName;
                        }

                        dirty = true;
                        break;
                    }

                    case SDL_EVENT_MOUSE_WHEEL: {
                        Camera* camera =
                            outputs.camera(
                                selectedOutput
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

                        const bool leftPane =
                            selectedOutput ==
                            laptopName;

                        const float paneX =
                            leftPane
                            ? 0.0F
                            : static_cast<float>(
                                width
                            ) / 2.0F;

                        const float localX =
                            mouseX -
                            paneX;

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
                                        localX
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

        if (gotEvent) {
            processEvent(event);

            while (
                SDL_PollEvent(
                    &event
                )
            ) {
                processEvent(event);
            }
        }

        if (!running) {
            break;
        }

        const auto now =
            Clock::now();

        double dt =
            std::chrono::duration<double>(
                now -
                previous
            ).count();

        previous = now;

        dt =
            std::clamp(
                dt,
                0.0,
                0.05
            );

        if (keys.active()) {
            Camera* camera =
                outputs.camera(
                    selectedOutput
                );

            if (camera != nullptr) {
                constexpr double speed =
                    1600.0;

                camera->moveBy(
                    direction(keys) *
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
                ) -
                    half,
                static_cast<float>(
                    height
                )
            };

            SDL_SetRenderDrawColor(
                renderer,
                10,
                12,
                16,
                255
            );

            SDL_RenderClear(
                renderer
            );

            renderOutput(
                renderer,
                world,
                outputs,
                laptopName,
                selectedOutput,
                left
            );

            renderOutput(
                renderer,
                world,
                outputs,
                externalName,
                selectedOutput,
                right
            );

            SDL_RenderPresent(
                renderer
            );

            updateTitle(
                window,
                outputs,
                selectedOutput
            );

            dirty = false;
        }
    }

    SDL_DestroyRenderer(
        renderer
    );

    SDL_DestroyWindow(
        window
    );

    SDL_Quit();

    return 0;
}
