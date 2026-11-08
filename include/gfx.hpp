#pragma once

// The whole public API: Image, the 2D Canvas and the 3D Renderer.
// The Renderer is only defined when the library is built with GFX_VULKAN=ON.

#include "image/image.hpp"
#include "canvas/canvas.hpp"
#include "scene/scene.hpp"
#include "scene/renderer.hpp"
