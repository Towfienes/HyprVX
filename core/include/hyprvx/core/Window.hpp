#pragma once

#include "Rect.hpp"
#include "Types.hpp"

namespace hyprvx {

struct WorldWindow {
    WindowId id{};
    Rect geometry{};
};

struct IslandMember {
    WindowId id{};
    Rect localGeometry{};
};

}
