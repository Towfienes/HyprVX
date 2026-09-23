#include <stdexcept>
#include <utility>

#include "hyprvx/core/OutputTopology.hpp"

namespace hyprvx {

bool OutputTopologySnapshot::connected(
    const std::string& name
) const noexcept {
    const auto it =
        outputs.find(name);

    return
        it != outputs.end() &&
        it->second.connected;
}

bool OutputTopologySnapshot::active(
    const std::string& name
) const noexcept {
    const auto it =
        outputs.find(name);

    return
        it != outputs.end() &&
        it->second.active;
}

OutputTopologyPlanner::
OutputTopologyPlanner(
    LaptopExternalTopologyPolicy policy
)
    : policy_(std::move(policy)) {

    if (
        policy_.laptopOutput.empty() ||
        policy_.externalOutput.empty() ||
        policy_.laptopOutput ==
            policy_.externalOutput
    ) {
        throw std::invalid_argument(
            "invalid output topology policy"
        );
    }
}

std::vector<OutputTopologyAction>
OutputTopologyPlanner::plan(
    const OutputTopologySnapshot&
        snapshot
) const {
    std::vector<OutputTopologyAction>
        actions;

    const bool laptopConnected =
        snapshot.connected(
            policy_.laptopOutput
        );

    const bool externalConnected =
        snapshot.connected(
            policy_.externalOutput
        );

    const bool laptopActive =
        snapshot.active(
            policy_.laptopOutput
        );

    const bool externalActive =
        snapshot.active(
            policy_.externalOutput
        );

    if (externalConnected) {
        if (!externalActive) {
            actions.push_back({
                OutputTopologyActionKind::
                    Activate,
                policy_.externalOutput
            });
        }

        if (
            !snapshot.primary.has_value() ||
            *snapshot.primary !=
                policy_.externalOutput
        ) {
            actions.push_back({
                OutputTopologyActionKind::
                    SelectPrimary,
                policy_.externalOutput
            });
        }

        if (
            policy_
                .disableLaptopWhenExternal &&
            laptopActive
        ) {
            actions.push_back({
                OutputTopologyActionKind::
                    Deactivate,
                policy_.laptopOutput
            });
        }

        return actions;
    }

    if (laptopConnected) {
        if (!laptopActive) {
            actions.push_back({
                OutputTopologyActionKind::
                    Activate,
                policy_.laptopOutput
            });
        }

        if (
            !snapshot.primary.has_value() ||
            *snapshot.primary !=
                policy_.laptopOutput
        ) {
            actions.push_back({
                OutputTopologyActionKind::
                    SelectPrimary,
                policy_.laptopOutput
            });
        }

        if (externalActive) {
            actions.push_back({
                OutputTopologyActionKind::
                    Deactivate,
                policy_.externalOutput
            });
        }

        return actions;
    }

    if (externalActive) {
        actions.push_back({
            OutputTopologyActionKind::
                Deactivate,
            policy_.externalOutput
        });
    }

    if (laptopActive) {
        actions.push_back({
            OutputTopologyActionKind::
                Deactivate,
            policy_.laptopOutput
        });
    }

    return actions;
}

}
