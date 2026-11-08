#pragma once

#include "scene/scene.hpp"

#include <memory>

namespace gfx::scene {
    class Renderer {
    public:
        Renderer();
        ~Renderer();
        Renderer(const Renderer &) = delete;
        Renderer &operator=(const Renderer &) = delete;
        Renderer(Renderer &&) noexcept;
        Renderer &operator=(Renderer &&) noexcept;

        Image render(const Scene &scene, Extent size, const Matrix &camera = identity);

    private:
        struct State;
        std::unique_ptr<State> state;
    };
}
