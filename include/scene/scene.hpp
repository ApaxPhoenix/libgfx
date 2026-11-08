#pragma once

#include "image/image.hpp"

#include <array>
#include <vector>

namespace gfx::scene {
    using Matrix = std::array<float, 16>;

    inline constexpr Matrix identity{
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    };

    struct Vertex {
        std::array<float, 3> position{};
        Color color;
    };

    struct Scene {
        std::vector<Vertex> vertices;
        std::vector<std::uint32_t> indices;
        Color background{0.0f, 0.0f, 0.0f, 1.0f};
    };
}
