#pragma once

#include "image/image.hpp"

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>

namespace gfx::canvas {
    struct Point {
        float horizontal;
        float vertical;
    };

    struct Paint {
        Color color;
        float width = 0.0f;
    };

    enum class Target : std::uint8_t { Raster, Svg };

    class Canvas {
    public:
        explicit Canvas(Extent size, Target target = Target::Raster);
        ~Canvas();
        Canvas(const Canvas &) = delete;
        Canvas &operator=(const Canvas &) = delete;
        Canvas(Canvas &&) noexcept;
        Canvas &operator=(Canvas &&) noexcept;

        void clear(Color color);
        void path(std::span<const Point> points, Paint paint, bool closed = false);
        void text(std::string_view text, Point origin, float size, Paint paint);
        void image(const Image &source, Point origin);
        Image image() const;
        std::string svg();

    private:
        struct State;
        std::unique_ptr<State> state;
    };
}
