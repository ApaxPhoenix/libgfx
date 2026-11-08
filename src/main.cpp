#include <gfx.hpp>

#include <GLFW/glfw3.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string_view>

namespace {
    constexpr gfx::Extent panel{360, 360};
    constexpr gfx::Extent extent{panel.width * 2, panel.height};

    struct Frame {
        gfx::scene::Renderer renderer;
        gfx::canvas::Canvas canvas{extent};
        gfx::scene::Scene scene{
            .vertices = {
                {{-0.50f, -0.40f, 0.0f}, {0.12f, 0.80f, 0.72f}},
                {{ 0.50f, -0.40f, 0.0f}, {0.12f, 0.80f, 0.72f}},
                {{ 0.00f,  0.60f, 0.0f}, {0.12f, 0.80f, 0.72f}}
            },
            .indices = {},
            .background ={0.07f, 0.09f, 0.14f}
        };

        gfx::Image draw(float progress) {
            constexpr float distance = 2.4f;
            constexpr float scale = 1.6f;
            constexpr float depth = 10.0f / 9.9f;
            const auto yaw = (progress - 0.5f) * 1.4f;
            const auto sine = std::sin(yaw);
            const auto cosine = std::cos(yaw);
            const auto translation = std::lerp(-0.84f, 0.84f, progress);
            const gfx::scene::Matrix camera{
                scale * cosine, 0, -depth * sine, -sine,
                0, scale, 0, 0,
                scale * sine, 0, depth * cosine, cosine,
                scale * translation, 0, depth * (distance - 0.1f), distance
            };
            const auto image = renderer.render(scene, panel, camera);

            canvas.clear({0.11f, 0.13f, 0.18f});
            canvas.image(image, {static_cast<float>(panel.width), 0});
            const std::array triangle{
                gfx::canvas::Point{110, 244},
                gfx::canvas::Point{250, 244},
                gfx::canvas::Point{180, 118}
            };
            canvas.path(triangle, {{0.96f, 0.52f, 0.20f}}, true);
            const gfx::canvas::Paint white{{0.92f, 0.95f, 1.0f}};
            canvas.text("Skia / 2D", {24, 42}, 22, white);
            canvas.text("Vulkan / Perspective", {384, 42}, 22, white);
            return canvas.image();
        }
    };

    std::size_t center(const gfx::Image &image) {
        std::size_t total = 0;
        std::size_t count = 0;
        const auto pixels = image.pixels();
        for (std::uint32_t row = 0; row < extent.height; ++row) {
            for (std::uint32_t column = panel.width; column < extent.width; ++column) {
                const auto offset = (row * image.stride()) + (static_cast<std::size_t>(column) * 4);
                if (pixels[offset] < 100 && pixels[offset + 1] > 150 && pixels[offset + 2] > 100) {
                    total += column;
                    ++count;
                }
            }
        }
        if (!count) throw std::runtime_error("Vulkan triangle is missing");
        return total / count;
    }

    void check(Frame &frame) {
        const auto left = frame.draw(0.0f);
        const auto right = frame.draw(1.0f);
        const auto first = center(left);
        const auto last = center(right);
        if (last <= first + 120) throw std::runtime_error("Vulkan triangle did not move right");

        const auto middle = (std::size_t{200} * left.stride()) + (std::size_t{180} * 4);
        if (left.pixels()[middle] < 220 || left.pixels()[middle + 2] > 80)
            throw std::runtime_error("Skia triangle is missing");

        constexpr auto panel_bytes = static_cast<std::size_t>(panel.width) * 4;
        for (std::uint32_t row = 0; row < extent.height; ++row) {
            const auto offset = row * left.stride();
            if (!std::ranges::equal(left.pixels().subspan(offset, panel_bytes),
                                    right.pixels().subspan(offset, panel_bytes)))
                throw std::runtime_error("Skia triangle did not stay fixed");
        }

        gfx::canvas::Canvas vector({32, 32}, gfx::canvas::Target::Svg);
        const std::array points{
            gfx::canvas::Point{2, 2}, gfx::canvas::Point{30, 2}, gfx::canvas::Point{15, 30}
        };
        vector.path(points, {{0.2f, 0.8f, 0.6f}}, true);
        const auto svg = vector.svg();
        if (!svg.contains("<svg") || !svg.contains("<path") || !svg.contains("</svg>"))
            throw std::runtime_error("SVG export failed");

        const auto png = left.png();
        if (png.size() < 8 || png[0] != 137 || png[1] != 'P' || png[2] != 'N' || png[3] != 'G')
            throw std::runtime_error("PNG export failed");
        std::cout << "Skia fixed, Vulkan moved from column " << first << " to " << last
                  << "; PNG and SVG passed\n";
    }

    // Terminates GLFW when the preview ends, however it ends.
    struct Session {
        Session() = default;
        ~Session() { glfwTerminate(); }
        Session(const Session &) = delete;
        Session &operator=(const Session &) = delete;
        Session(Session &&) = delete;
        Session &operator=(Session &&) = delete;
    };

    void preview(Frame &frame, bool once) {
        if (!glfwInit()) throw std::runtime_error("could not initialize GLFW");
        const Session session;

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
        if (once) glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        const std::unique_ptr<GLFWwindow, decltype(&glfwDestroyWindow)> window(
            glfwCreateWindow(static_cast<int>(extent.width), static_cast<int>(extent.height),
                             "libgfx frame", nullptr, nullptr), glfwDestroyWindow);
        if (!window) throw std::runtime_error("could not open GLFW preview");
        glfwMakeContextCurrent(window.get());
        glfwSwapInterval(1);

        unsigned texture = 0;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, extent.width, extent.height,
                     0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glEnable(GL_TEXTURE_2D);
        const auto start = glfwGetTime();
        unsigned frames = 0;
        do {
            const auto progress = once ? static_cast<float>(frames)
                                       : static_cast<float>(std::fmod(glfwGetTime() - start, 3.0) / 3.0);
            const auto image = frame.draw(progress);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, extent.width, extent.height,
                            GL_RGBA, GL_UNSIGNED_BYTE, image.pixels().data());
            int width = 0;
            int height = 0;
            glfwGetFramebufferSize(window.get(), &width, &height);
            glViewport(0, 0, width, height);
            glBegin(GL_QUADS);
            glTexCoord2f(0, 1); glVertex2f(-1, -1);
            glTexCoord2f(1, 1); glVertex2f( 1, -1);
            glTexCoord2f(1, 0); glVertex2f( 1,  1);
            glTexCoord2f(0, 0); glVertex2f(-1,  1);
            glEnd();
            glfwSwapBuffers(window.get());
            glfwPollEvents();
            if (glfwGetKey(window.get(), GLFW_KEY_ESCAPE) == GLFW_PRESS)
                glfwSetWindowShouldClose(window.get(), GLFW_TRUE);
            ++frames;
        } while ((!once || frames < 2) && !glfwWindowShouldClose(window.get()));

        glDeleteTextures(1, &texture);
        if (glGetError() != GL_NO_ERROR) throw std::runtime_error("could not display the GLFW frame");
        if (once) std::cout << "GLFW preview passed\n";
    }
}

int main(int count, char **arguments) try {
    const std::string_view option = count == 2 ? arguments[1] : "";
    if (count > 2 || (!option.empty() && option != "--check" &&
                     option != "--preview" && option != "--preview-once"))
        throw std::invalid_argument("usage: libgfx [--check|--preview|--preview-once]");

    Frame frame;
    if (option == "--check") {
        check(frame);
    } else if (option == "--preview" || option == "--preview-once") {
        preview(frame, option == "--preview-once");
    } else {
        const auto bytes = frame.draw(0.5f).png();
        std::ofstream file("frame.png", std::ios::binary);
        file.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        file.close();
        if (!file) throw std::runtime_error("could not write frame.png");
        std::cout << "Wrote frame.png\n";
    }
    return EXIT_SUCCESS;
} catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return EXIT_FAILURE;
}
