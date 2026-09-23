#include <cmath>
#include <stdexcept>
#include <utility>

#include "hyprvx/core/OutputState.hpp"

namespace hyprvx {

void MultiOutputState::validate(
    const OutputDescriptor& descriptor
) {
    if (
        descriptor.name.empty() ||

        !std::isfinite(
            descriptor.desktopPosition.x
        ) ||
        !std::isfinite(
            descriptor.desktopPosition.y
        ) ||

        !std::isfinite(
            descriptor.viewportSize.x
        ) ||
        !std::isfinite(
            descriptor.viewportSize.y
        ) ||

        !(descriptor.viewportSize.x > 0.0) ||
        !(descriptor.viewportSize.y > 0.0) ||

        !std::isfinite(
            descriptor.scale
        ) ||
        !(descriptor.scale > 0.0) ||

        !std::isfinite(
            descriptor.refreshHz
        ) ||
        !(descriptor.refreshHz > 0.0)
    ) {
        throw std::invalid_argument(
            "invalid output descriptor"
        );
    }
}

bool MultiOutputState::activate(
    OutputDescriptor descriptor,
    Camera fallbackCamera
) {
    validate(descriptor);

    if (
        active_.contains(
            descriptor.name
        )
    ) {
        return false;
    }

    Camera outputCamera =
        fallbackCamera;

    const auto stored =
        storedCameras_.find(
            descriptor.name
        );

    if (
        stored !=
        storedCameras_.end()
    ) {
        outputCamera =
            stored->second;

        storedCameras_.erase(
            stored
        );
    }

    const std::string key =
        descriptor.name;

    active_.emplace(
        key,
        OutputState{
            std::move(descriptor),
            outputCamera
        }
    );

    return true;
}

bool MultiOutputState::deactivate(
    const std::string& name
) {
    const auto it =
        active_.find(name);

    if (it == active_.end()) {
        return false;
    }

    storedCameras_.insert_or_assign(
        name,
        it->second.camera
    );

    active_.erase(it);

    return true;
}

bool MultiOutputState::updateDescriptor(
    OutputDescriptor descriptor
) {
    validate(descriptor);

    const auto it =
        active_.find(
            descriptor.name
        );

    if (it == active_.end()) {
        return false;
    }

    it->second.descriptor =
        std::move(descriptor);

    return true;
}

OutputState* MultiOutputState::find(
    const std::string& name
) noexcept {
    const auto it =
        active_.find(name);

    return
        it == active_.end()
        ? nullptr
        : &it->second;
}

const OutputState* MultiOutputState::find(
    const std::string& name
) const noexcept {
    const auto it =
        active_.find(name);

    return
        it == active_.end()
        ? nullptr
        : &it->second;
}

Camera* MultiOutputState::camera(
    const std::string& name
) noexcept {
    OutputState* output =
        find(name);

    return
        output == nullptr
        ? nullptr
        : &output->camera;
}

const Camera* MultiOutputState::camera(
    const std::string& name
) const noexcept {
    const OutputState* output =
        find(name);

    return
        output == nullptr
        ? nullptr
        : &output->camera;
}

bool MultiOutputState::isActive(
    const std::string& name
) const noexcept {
    return active_.contains(name);
}

bool MultiOutputState::hasStoredCamera(
    const std::string& name
) const noexcept {
    return
        storedCameras_.contains(name);
}

}
