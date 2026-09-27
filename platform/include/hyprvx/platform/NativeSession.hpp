#pragma once

#include "hyprvx/platform/NestedGuard.hpp"

#include <filesystem>
#include <memory>

namespace hyprvx::platform {

struct NativeSessionOptions {
    bool interactive{};
    bool diagnostics{};
    std::filesystem::path statePath;
};

class NativeSession {
public:
    NativeSession(NestedContract contract, NativeSessionOptions options);
    ~NativeSession();
    NativeSession(const NativeSession&) = delete;
    NativeSession& operator=(const NativeSession&) = delete;
    int run();
private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace hyprvx::platform
