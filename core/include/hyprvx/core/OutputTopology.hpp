#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace hyprvx {

struct OutputRuntimeState {
    bool connected{};
    bool active{};
};

struct OutputTopologySnapshot {
    std::unordered_map<
        std::string,
        OutputRuntimeState
    > outputs{};

    std::optional<std::string> primary{};

    [[nodiscard]]
    bool connected(
        const std::string& name
    ) const noexcept;

    [[nodiscard]]
    bool active(
        const std::string& name
    ) const noexcept;
};

enum class OutputTopologyActionKind {
    Activate,
    Deactivate,
    SelectPrimary
};

struct OutputTopologyAction {
    OutputTopologyActionKind kind{};
    std::string output{};

    bool operator==(
        const OutputTopologyAction&
    ) const = default;
};

struct LaptopExternalTopologyPolicy {
    std::string laptopOutput{
        "eDP-2"
    };

    std::string externalOutput{
        "DP-1"
    };

    bool disableLaptopWhenExternal{
        true
    };
};

class OutputTopologyPlanner {
public:
    explicit OutputTopologyPlanner(
        LaptopExternalTopologyPolicy
            policy = {}
    );

    [[nodiscard]]
    const LaptopExternalTopologyPolicy&
    policy() const noexcept {
        return policy_;
    }

    [[nodiscard]]
    std::vector<OutputTopologyAction>
    plan(
        const OutputTopologySnapshot&
            snapshot
    ) const;

private:
    LaptopExternalTopologyPolicy
        policy_{};
};

}
