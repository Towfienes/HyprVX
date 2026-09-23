#pragma once

#include <optional>
#include <string>
#include <unordered_map>

#include "Camera.hpp"

namespace hyprvx {

struct OutputDescriptor {
    std::string name{};

    Vec2 desktopPosition{};
    Vec2 viewportSize{};

    double scale{1.0};
    double refreshHz{60.0};
};

struct OutputState {
    OutputDescriptor descriptor{};
    Camera camera{};
};

class MultiOutputState {
public:
    bool activate(
        OutputDescriptor descriptor,
        Camera fallbackCamera = {}
    );

    bool deactivate(
        const std::string& name
    );

    bool updateDescriptor(
        OutputDescriptor descriptor
    );

    [[nodiscard]]
    OutputState* find(
        const std::string& name
    ) noexcept;

    [[nodiscard]]
    const OutputState* find(
        const std::string& name
    ) const noexcept;

    [[nodiscard]]
    Camera* camera(
        const std::string& name
    ) noexcept;

    [[nodiscard]]
    const Camera* camera(
        const std::string& name
    ) const noexcept;

    [[nodiscard]]
    bool isActive(
        const std::string& name
    ) const noexcept;

    [[nodiscard]]
    bool hasStoredCamera(
        const std::string& name
    ) const noexcept;

    [[nodiscard]]
    std::size_t activeCount() const noexcept {
        return active_.size();
    }

    [[nodiscard]]
    std::size_t storedCount() const noexcept {
        return storedCameras_.size();
    }

private:
    static void validate(
        const OutputDescriptor& descriptor
    );

    std::unordered_map<
        std::string,
        OutputState
    > active_{};

    std::unordered_map<
        std::string,
        Camera
    > storedCameras_{};
};

}
