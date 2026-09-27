#include "hyprvx/platform/NestedGuard.hpp"

#include <exception>
#include <iostream>

int main() {
    try {
        const auto contract = hyprvx::platform::requireNestedEnvironment();
        std::cout << "nested instance verified: " << contract.signature << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "nested guard refused: " << error.what() << '\n';
        return 2;
    }
}
