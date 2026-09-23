#include <iomanip>
#include <iostream>

#include "hyprvx/core/Camera.hpp"
#include "hyprvx/core/CameraMotion.hpp"
#include "hyprvx/core/Island.hpp"
#include "hyprvx/core/World.hpp"

namespace {

hyprvx::Vec2 simulatePan(
    int refreshRate
) {
    using namespace hyprvx;

    Camera camera;

    CameraMotionConfig config;
    config.acceleration = 1800.0;
    config.damping = 5.5;
    config.maxSpeed = 1000000.0;

    CameraController controller{
        camera,
        config
    };

    controller.setPanInput({
        1.0,
        0.25
    });

    const double dt =
        1.0 /
        static_cast<double>(
            refreshRate
        );

    for (int i = 0; i < refreshRate; ++i) {
        controller.update(dt);
    }

    return camera.position();
}

}

int main() {
    using namespace hyprvx;

    std::cout
        << "HyprVX Phase 1B camera simulator\n\n";

    const Vec2 p60 =
        simulatePan(60);

    const Vec2 p120 =
        simulatePan(120);

    const Vec2 p210 =
        simulatePan(210);

    std::cout
        << std::fixed
        << std::setprecision(6);

    std::cout
        << "60Hz  camera = "
        << p60.x
        << ", "
        << p60.y
        << "\n";

    std::cout
        << "120Hz camera = "
        << p120.x
        << ", "
        << p120.y
        << "\n";

    std::cout
        << "210Hz camera = "
        << p210.x
        << ", "
        << p210.y
        << "\n";

    std::cout
        << "\nframe-rate delta 60->210 = "
        << (p60 - p210).length()
        << "\n";

    Camera camera{
        {-1200.0, 800.0},
        1.0
    };

    World world;

    world.addFreeWindow({
        1,
        {
            {-800.0, 1000.0},
            {900.0, 600.0}
        }
    });

    Island code{
        10,
        {2500.0, -1500.0}
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

    const auto* freeWindow =
        world.freeWindow(1);

    if (freeWindow != nullptr) {
        const Vec2 screen =
            camera.worldToScreen(
                freeWindow->geometry.position
            );

        std::cout
            << "\nfree window screen origin = "
            << screen.x
            << ", "
            << screen.y
            << "\n";
    }

    const Island* island =
        world.island(10);

    if (island != nullptr) {
        const auto member =
            island->memberWorldGeometry(2);

        if (member.has_value()) {
            const Vec2 screen =
                camera.worldToScreen(
                    member->position
                );

            std::cout
                << "island member screen origin = "
                << screen.x
                << ", "
                << screen.y
                << "\n";
        }
    }

    CameraController flight{
        camera
    };

    flight.flyTo({
        2500.0,
        -1500.0
    });

    for (int i = 0; i < 120; ++i) {
        flight.update(
            1.0 / 120.0
        );
    }

    std::cout
        << "\ncamera after 1s flight = "
        << camera.position().x
        << ", "
        << camera.position().y
        << "\n";

    return 0;
}
