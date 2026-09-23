#include <iostream>
#include <utility>

#include "hyprvx/core/Camera.hpp"
#include "hyprvx/core/Island.hpp"
#include "hyprvx/core/World.hpp"

int main() {
    using namespace hyprvx;

    Camera camera{
        {1000.0, -500.0},
        1.25
    };

    World world;

    world.addFreeWindow({
        1,
        {
            {1250.0, -300.0},
            {900.0, 600.0}
        }
    });

    Island code{
        10,
        {4000.0, 2000.0}
    };

    code.addMember({
        2,
        {
            {0.0, 0.0},
            {1200.0, 800.0}
        }
    });

    code.addMember({
        3,
        {
            {1240.0, 0.0},
            {900.0, 800.0}
        }
    });

    world.addIsland(
        std::move(code)
    );

    const Vec2 screen =
        camera.worldToScreen({
            1250.0,
            -300.0
        });

    std::cout
        << "HyprVX Phase 1A simulator\n";

    std::cout
        << "free_windows="
        << world.freeWindowCount()
        << " islands="
        << world.islandCount()
        << "\n";

    std::cout
        << "sample_screen="
        << screen.x
        << ","
        << screen.y
        << "\n";

    return 0;
}
