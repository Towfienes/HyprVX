#include "hyprvx/platform/NativeSession.hpp"

#include <exception>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    try {
        hyprvx::platform::NativeSessionOptions options;
        std::string expectedInstance;
        bool nestedOnly = false;
        for (int index = 1; index < argc; ++index) {
            const std::string argument = argv[index];
            if (argument == "--nested-only") nestedOnly = true;
            else if (argument == "--interactive") options.interactive = true;
            else if (argument == "--passive") options.interactive = false;
            else if (argument == "--diagnostics") options.diagnostics = true;
            else if (argument == "--instance" && index + 1 < argc) expectedInstance = argv[++index];
            else if (argument == "--state" && index + 1 < argc) options.statePath = argv[++index];
            else if (argument == "--help") {
                std::cout << "Usage: hyprvx-native-dev --nested-only [--instance SIGNATURE]"
                             " [--passive|--interactive] [--state FILE] [--diagnostics]\n";
                return 0;
            } else throw std::runtime_error("invalid native-dev argument: " + argument);
        }
        if (!nestedOnly) throw std::runtime_error("--nested-only is required");
        auto contract = hyprvx::platform::requireNestedEnvironment();
        if (!expectedInstance.empty() && expectedInstance != contract.signature)
            throw std::runtime_error("--instance does not match guarded nested compositor");
        hyprvx::platform::NativeSession session(std::move(contract), options);
        return session.run();
    } catch (const std::exception& error) {
        std::cerr << "hyprvx-native-dev refused/failed: " << error.what() << '\n';
        return 2;
    }
}
