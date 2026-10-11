#pragma once

// Included first by every spike source. The copied upstream function bodies (RenderHooks.cpp, InputHooks.cpp) use
// private and protected members exactly as the patched originals do; a plugin can reach them only by compiling the
// Hyprland headers with every member public. Standard headers are included before the redefinition. This is one of
// the fragility costs measured by the spike (delivery-decision.md).

#include <algorithm>
#include <any>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <format>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <ranges>
#include <set>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

#define private   public
#define protected public
