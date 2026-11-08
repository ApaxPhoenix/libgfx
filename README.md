# libgfx

**Headless 2D and 3D drawing for C++26. No window, no display, just pixels.**

libgfx is a small library for drawing pictures in programs that have no screen
to draw on. A `Canvas` draws 2D paths, text and images with Skia, to pixels or
to SVG. A `Renderer` draws a depth-tested triangle mesh with Vulkan and hands
back the pixels. Both produce the same `Image`, so a 3D render can be placed on
a 2D canvas, saved as a PNG, or shown in a window you already have.

It is meant for the places a graphics library usually doesn't fit: tests,
servers, command-line tools and anything else that needs a picture without
opening a window.

> **Status: early development (0.1.0).** The API is small and still changing,
> and nothing has been tagged yet. It is tested on macOS only.

## What it's going to be

- **Drawing that works anywhere.** The same code draws on a laptop, in CI and
  on a server with a GPU and no monitor.
- **One image type.** Everything ends as plain RGBA8 pixels that any other
  library, file format or window can take.
- **2D and 3D together.** Vector graphics and a real GPU renderer behind one
  small API, so a chart, a label and a 3D view can end up in the same picture.
- **Small on purpose.** A handful of types, readable in an afternoon, with the
  heavy lifting left to Skia and Vulkan.

## How it works

1. **Everything is an `Image`.** `gfx::Image` holds tightly packed, top-down
   RGBA8 pixels with straight alpha, and can encode itself as PNG.
2. **2D goes through Skia.** A `Canvas` owns a Skia raster surface, or, with
   `Target::Svg`, a Skia SVG canvas that writes a document instead of pixels.
   Coordinates are pixels, with the origin at the upper left.
3. **3D goes through Vulkan, off-screen.** A `Renderer` picks a Vulkan 1.1
   device and builds its pipeline once. Each frame draws into color and depth
   images in GPU memory, copies the result into a buffer the CPU can read, and
   returns it as an `Image`. No surface or swapchain is ever created.
4. **Shaders are compiled at build time.** The Slang shaders in `shaders/` are
   compiled to SPIR-V by `slangc` during the build and embedded in the library,
   so nothing is read from disk at runtime.
5. **The camera is yours.** A frame takes a column-major clip transform with
   Vulkan's depth range of zero to one. libgfx doesn't impose a camera model.

## Features

**Images**
- RGBA8 pixels, readable and writable as a `std::span`.
- PNG encoding.

**2D (`gfx::canvas`)**
- Paths of any number of points, open or closed, filled or stroked.
- Text in the system's default font, at any size.
- Images, including the output of the 3D renderer.
- SVG output from the same drawing calls.

**3D (`gfx::scene`)**
- A triangle mesh, indexed or not, with a color per vertex.
- Depth testing and a background color.
- Any frame size the device supports.
- A `Renderer` can be reused for later frames, and used from several threads;
  its frames are drawn one at a time.

**Build**
- One header: `#include <gfx.hpp>`.
- A Skia-only build (`GFX_VULKAN=OFF`) for programs that don't need 3D.
- CMake presets, clang-tidy and clang-format configured.

## How it compares

|  | libgfx | Skia | Cairo | raylib |
| --- | :---: | :---: | :---: | :---: |
| 2D vector drawing | ✅ | ✅ | ✅ | ✅ |
| SVG output | ✅ | ✅ | ✅ | ❌ |
| 3D meshes | ✅ | ❌ | ❌ | ✅ |
| Works without a window | ✅ | ✅ | ✅ | ❌ |
| Vulkan | ✅ | ✅ | ❌ | ❌ |
| 2D and 3D in one API | ✅ | ❌ | ❌ | ✅ |
| Textures, lighting, materials | ❌ | n/a | n/a | ✅ |

libgfx is built on Skia, so it can only do less in 2D than Skia itself. What it
adds is a 3D renderer beside it and one image type between them. If you need a
full game engine, raylib does far more.

This table is our best reading of each project's documentation and may fall
out of date. If something is wrong, please open an issue.

## Roadmap

**Next**
- Tests for each module, beside the demo's check
- Testing on Linux and Windows
- Choosing a font for text

**Later**
- Textures and simple lighting in the renderer
- More than one mesh per frame
- Shaders supplied at runtime, compiled with Slang
- A Vulkan pipeline cache, so the first frame starts faster

## Platforms

| Platform | Status |
| --- | --- |
| macOS (Apple silicon) | Works. This is where development happens. Vulkan runs through MoltenVK. |
| Linux | Expected to work; the code has paths for its fonts. Not tested yet. |
| Windows | Expected to work; the code has paths for its fonts. Not tested yet. |

There are no releases yet. Changes are listed in [CHANGELOG.md](CHANGELOG.md).

## Building from source

You need CMake 4.2 or newer, Ninja, and a C++26 compiler. On macOS that is
Homebrew's LLVM (`brew install llvm`); Apple's clang is too old. You also need
Skia from vcpkg (`vcpkg install skia`), and, for the 3D renderer, the Vulkan
SDK and `slangc`. CMake downloads GLFW for the demo if it isn't installed.

```bash
git clone https://github.com/ApaxPhoenix/libgfx.git
cd libgfx
cmake --preset release
cmake --build --preset release
./bin/libgfx
```

`./bin/libgfx` writes `frame.png`. `./bin/libgfx --preview` opens a window
with a fixed Skia triangle beside a Vulkan triangle that moves and turns.

### Configuration

The presets in `CMakePresets.json` expect vcpkg in `~/vcpkg` and `slangc` in
`../libsci/vendors/slang/bin`. Put a `CMakeUserPresets.json` next to them to use
other paths.

| Preset | What it builds |
| --- | --- |
| `release` | The optimised build. |
| `debug` | No optimisation, with debug information. Links the release Skia. |
| `tidy` | The debug build, with clang-tidy run on every file. |

| Option | Default | What it's for |
| --- | --- | --- |
| `GFX_VULKAN` | `ON` | The 3D renderer. `OFF` leaves a Skia-only library. |
| `GFX_EXAMPLES` | `ON` when top-level | The demo. Needs `GFX_VULKAN`. |
| `GFX_CLANG_TIDY` | `OFF` | Run clang-tidy during the build. |

### Using it in your project

```cmake
add_subdirectory(libgfx)
target_link_libraries(app PRIVATE gfx::gfx)
```

```cpp
#include <gfx.hpp>

gfx::scene::Renderer renderer;
const gfx::scene::Scene scene{
    .vertices = {
        {{-0.5f, -0.4f, 0.0f}, {0.12f, 0.80f, 0.72f}},
        {{ 0.5f, -0.4f, 0.0f}, {0.12f, 0.80f, 0.72f}},
        {{ 0.0f,  0.6f, 0.0f}, {0.12f, 0.80f, 0.72f}},
    },
    .indices = {},
    .background = {0.07f, 0.09f, 0.14f},
};
const auto mesh = renderer.render(scene, {360, 360});

gfx::canvas::Canvas canvas({720, 360});
canvas.clear({0.11f, 0.13f, 0.18f});
canvas.image(mesh, {360, 0});
canvas.text("Skia + Vulkan", {24, 42}, 22, {{0.92f, 0.95f, 1.0f}});
const auto png = canvas.image().png();
```

### Tests

```bash
ctest --preset release
```

## Security

Found a vulnerability? Please report it privately through the repository's
**Security** tab and read [SECURITY.md](SECURITY.md) before disclosing anything
publicly.

## Contributing

Contributions of every size are welcome. Please open an issue before starting
on a large change. Pull requests should build without warnings, pass `ctest`,
build cleanly with the `tidy` preset, and follow `.clang-format`.

## License

libgfx does not have a license yet, so all rights are reserved until one is
added. Skia, Vulkan, Slang and GLFW keep their own licenses.
