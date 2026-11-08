# Changelog

All notable changes to libgfx are documented in this file.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the project uses
[Semantic Versioning](https://semver.org/spec/v2.0.0.html). Nothing has been tagged yet; the
version in `CMakeLists.txt` is 0.1.0.

## [Unreleased]

### Added

- `gfx.hpp`, a single header for the whole API.
- `gfx::Image`: tightly packed, top-down RGBA8 pixels with straight alpha, and PNG encoding.
- `gfx::canvas::Canvas`: 2D paths, text and images with Skia, to pixels or SVG.
- `gfx::scene::Renderer`: a depth-tested triangle mesh drawn headlessly with Vulkan, with Slang
  shaders compiled by `slangc` at build time and embedded as SPIR-V.
- `libgfx` demo (`src/main.cpp`): writes `frame.png`, or opens a GLFW preview window.
- `.clang-tidy`, `.clang-format` and `CMakePresets.json`.
