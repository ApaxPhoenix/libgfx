#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace gfx {
    struct Extent {
        std::uint32_t width;
        std::uint32_t height;
    };

    struct Color {
        float red = 0.0f;
        float green = 0.0f;
        float blue = 0.0f;
        float alpha = 1.0f;
    };

    class Image {
    public:
        explicit Image(Extent size);

        Extent size() const noexcept { return extent; }
        std::size_t stride() const noexcept { return static_cast<std::size_t>(extent.width) * 4; }
        std::span<std::uint8_t> pixels() noexcept { return storage; }
        std::span<const std::uint8_t> pixels() const noexcept { return storage; }
        std::vector<std::uint8_t> png() const;

    private:
        Extent extent;
        std::vector<std::uint8_t> storage;
    };
}
