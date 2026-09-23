#include <gtest/gtest.h>

#include "hyprvx/core/OutputState.hpp"

using namespace hyprvx;

namespace {

OutputDescriptor laptop() {
    return {
        "eDP-2",
        {0.0, 0.0},
        {2880.0, 1800.0},
        1.0,
        120.0
    };
}

OutputDescriptor external() {
    return {
        "DP-1",
        {2880.0, 0.0},
        {2560.0, 1440.0},
        1.0,
        210.0
    };
}

}

TEST(
    MultiOutputStateTest,
    ActivatesOutputs
) {
    MultiOutputState outputs;

    EXPECT_TRUE(
        outputs.activate(
            laptop()
        )
    );

    EXPECT_TRUE(
        outputs.activate(
            external()
        )
    );

    EXPECT_EQ(
        outputs.activeCount(),
        2u
    );

    EXPECT_TRUE(
        outputs.isActive(
            "eDP-2"
        )
    );

    EXPECT_TRUE(
        outputs.isActive(
            "DP-1"
        )
    );
}

TEST(
    MultiOutputStateTest,
    CamerasAreIndependent
) {
    MultiOutputState outputs;

    ASSERT_TRUE(
        outputs.activate(
            laptop()
        )
    );

    ASSERT_TRUE(
        outputs.activate(
            external()
        )
    );

    Camera* laptopCamera =
        outputs.camera(
            "eDP-2"
        );

    Camera* externalCamera =
        outputs.camera(
            "DP-1"
        );

    ASSERT_NE(
        laptopCamera,
        nullptr
    );

    ASSERT_NE(
        externalCamera,
        nullptr
    );

    laptopCamera->setPosition({
        100.0,
        200.0
    });

    externalCamera->setPosition({
        5000.0,
        -3000.0
    });

    EXPECT_DOUBLE_EQ(
        laptopCamera->position().x,
        100.0
    );

    EXPECT_DOUBLE_EQ(
        externalCamera->position().x,
        5000.0
    );
}

TEST(
    MultiOutputStateTest,
    DeactivateStoresCamera
) {
    MultiOutputState outputs;

    ASSERT_TRUE(
        outputs.activate(
            external()
        )
    );

    Camera* camera =
        outputs.camera(
            "DP-1"
        );

    ASSERT_NE(
        camera,
        nullptr
    );

    camera->setPosition({
        4200.0,
        -1200.0
    });

    camera->setZoom(
        0.75
    );

    EXPECT_TRUE(
        outputs.deactivate(
            "DP-1"
        )
    );

    EXPECT_FALSE(
        outputs.isActive(
            "DP-1"
        )
    );

    EXPECT_TRUE(
        outputs.hasStoredCamera(
            "DP-1"
        )
    );
}

TEST(
    MultiOutputStateTest,
    ReactivateRestoresCamera
) {
    MultiOutputState outputs;

    ASSERT_TRUE(
        outputs.activate(
            external()
        )
    );

    Camera* camera =
        outputs.camera(
            "DP-1"
        );

    ASSERT_NE(camera, nullptr);

    camera->setPosition({
        4200.0,
        -1200.0
    });

    camera->setZoom(
        0.75
    );

    ASSERT_TRUE(
        outputs.deactivate(
            "DP-1"
        )
    );

    ASSERT_TRUE(
        outputs.activate(
            external()
        )
    );

    const Camera* restored =
        outputs.camera(
            "DP-1"
        );

    ASSERT_NE(
        restored,
        nullptr
    );

    EXPECT_DOUBLE_EQ(
        restored->position().x,
        4200.0
    );

    EXPECT_DOUBLE_EQ(
        restored->position().y,
        -1200.0
    );

    EXPECT_DOUBLE_EQ(
        restored->zoom(),
        0.75
    );
}

TEST(
    MultiOutputStateTest,
    DescriptorUpdatePreservesCamera
) {
    MultiOutputState outputs;

    ASSERT_TRUE(
        outputs.activate(
            external()
        )
    );

    Camera* camera =
        outputs.camera(
            "DP-1"
        );

    ASSERT_NE(camera, nullptr);

    camera->setPosition({
        3000.0,
        4000.0
    });

    OutputDescriptor changed =
        external();

    changed.refreshHz =
        144.0;

    changed.scale =
        1.25;

    ASSERT_TRUE(
        outputs.updateDescriptor(
            changed
        )
    );

    const OutputState* output =
        outputs.find(
            "DP-1"
        );

    ASSERT_NE(
        output,
        nullptr
    );

    EXPECT_DOUBLE_EQ(
        output->descriptor.refreshHz,
        144.0
    );

    EXPECT_DOUBLE_EQ(
        output->descriptor.scale,
        1.25
    );

    EXPECT_DOUBLE_EQ(
        output->camera.position().x,
        3000.0
    );
}

TEST(
    MultiOutputStateTest,
    SupportsNegativeDesktopCoordinates
) {
    MultiOutputState outputs;

    OutputDescriptor left =
        external();

    left.desktopPosition =
        {-2560.0, 0.0};

    EXPECT_TRUE(
        outputs.activate(
            left
        )
    );

    const OutputState* output =
        outputs.find(
            "DP-1"
        );

    ASSERT_NE(
        output,
        nullptr
    );

    EXPECT_DOUBLE_EQ(
        output->descriptor
            .desktopPosition.x,
        -2560.0
    );
}

TEST(
    MultiOutputStateTest,
    SupportsMixedRefreshAndScale
) {
    MultiOutputState outputs;

    OutputDescriptor a =
        laptop();

    a.scale = 1.5;
    a.refreshHz = 120.0;

    OutputDescriptor b =
        external();

    b.scale = 1.0;
    b.refreshHz = 210.0;

    EXPECT_TRUE(
        outputs.activate(a)
    );

    EXPECT_TRUE(
        outputs.activate(b)
    );

    EXPECT_DOUBLE_EQ(
        outputs.find("eDP-2")
            ->descriptor.scale,
        1.5
    );

    EXPECT_DOUBLE_EQ(
        outputs.find("DP-1")
            ->descriptor.refreshHz,
        210.0
    );
}

TEST(
    MultiOutputStateTest,
    RejectsInvalidDescriptor
) {
    MultiOutputState outputs;

    OutputDescriptor broken =
        laptop();

    broken.viewportSize.x =
        0.0;

    EXPECT_THROW(
        outputs.activate(
            broken
        ),
        std::invalid_argument
    );

    broken =
        laptop();

    broken.refreshHz =
        -120.0;

    EXPECT_THROW(
        outputs.activate(
            broken
        ),
        std::invalid_argument
    );
}
