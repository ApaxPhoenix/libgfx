#include "canvas/canvas.hpp"

#include "include/core/SkCanvas.h"
#include "include/core/SkData.h"
#include "include/core/SkFont.h"
#include "include/core/SkFontMgr.h"
#include "include/core/SkImage.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPathBuilder.h"
#include "include/core/SkPixmap.h"
#include "include/core/SkStream.h"
#include "include/core/SkSurface.h"
#include "include/svg/SkSVGCanvas.h"

#if defined(__APPLE__)
#include "include/ports/SkFontMgr_mac_ct.h"
#elif defined(_WIN32)
#include "include/ports/SkTypeface_win.h"
#elif defined(__linux__)
#include "include/core/SkFontScanner.h"
#include "include/ports/SkFontMgr_fontconfig.h"
#include "include/ports/SkFontScanner_FreeType.h"
#endif

#include <limits>
#include <stdexcept>

namespace gfx::canvas {
    struct Canvas::State {
        Extent extent;
        sk_sp<SkSurface> surface;
        SkDynamicMemoryWStream stream;
        std::unique_ptr<SkCanvas> vector;
        std::string document;

        State(Extent size, Target target) : extent(size) {
            if (size.width == 0 || size.height == 0 ||
                size.width > std::numeric_limits<int>::max() ||
                size.height > std::numeric_limits<int>::max())
                throw std::invalid_argument("invalid canvas dimensions");

            if (target == Target::Raster) {
                surface = SkSurfaces::Raster(info(kPremul_SkAlphaType));
                if (!surface) throw std::runtime_error("could not create raster canvas");
            } else {
                vector = SkSVGCanvas::Make(SkRect::MakeWH(static_cast<float>(size.width), static_cast<float>(size.height)), &stream, 0);
                if (!vector) throw std::runtime_error("could not create SVG canvas");
            }
        }

        SkImageInfo info(SkAlphaType alpha) const {
            return SkImageInfo::Make(static_cast<int>(extent.width), static_cast<int>(extent.height),
                                     kRGBA_8888_SkColorType, alpha);
        }

        SkCanvas &drawing() const {
            if (surface) return *surface->getCanvas();
            if (vector) return *vector;
            throw std::logic_error("SVG canvas has been finalized");
        }

        static SkColor4f color(Color color) {
            return {color.red, color.green, color.blue, color.alpha};
        }

        static SkPaint paint(Paint paint) {
            SkPaint result;
            result.setAntiAlias(true);
            result.setColor(color(paint.color));
            if (paint.width > 0) {
                result.setStyle(SkPaint::kStroke_Style);
                result.setStrokeWidth(paint.width);
            }
            return result;
        }

        static sk_sp<SkTypeface> typeface() {
#if defined(__APPLE__)
            const auto fonts = SkFontMgr_New_CoreText(nullptr);
#elif defined(_WIN32)
            const auto fonts = SkFontMgr_New_DirectWrite();
#elif defined(__linux__)
            const auto fonts = SkFontMgr_New_FontConfig(nullptr, SkFontScanner_Make_FreeType());
#else
            const auto fonts = SkFontMgr::RefEmpty();
#endif
            return fonts ? fonts->legacyMakeTypeface(nullptr, SkFontStyle::Normal()) : nullptr;
        }
    };

    Canvas::Canvas(Extent size, Target target) : state(std::make_unique<State>(size, target)) {}
    Canvas::~Canvas() = default;
    Canvas::Canvas(Canvas &&) noexcept = default;
    Canvas &Canvas::operator=(Canvas &&) noexcept = default;

    void Canvas::clear(Color color) {
        state->drawing().clear(State::color(color));
    }

    void Canvas::path(std::span<const Point> points, Paint paint, bool closed) {
        if (points.empty()) return;
        SkPathBuilder path;
        path.moveTo(points.front().horizontal, points.front().vertical);
        for (const auto &point : points.subspan(1))
            path.lineTo(point.horizontal, point.vertical);
        if (closed) path.close();
        state->drawing().drawPath(path.detach(), State::paint(paint));
    }

    void Canvas::text(std::string_view text, Point origin, float size, Paint paint) {
        static const auto typeface = State::typeface();
        if (!typeface) throw std::runtime_error("could not load a default font");
        const SkFont font(typeface, size);
        state->drawing().drawSimpleText(text.data(), text.size(), SkTextEncoding::kUTF8,
                                       origin.horizontal, origin.vertical, font, State::paint(paint));
    }

    void Canvas::image(const Image &source, Point origin) {
        const auto size = source.size();
        const auto info = SkImageInfo::Make(static_cast<int>(size.width), static_cast<int>(size.height),
                                            kRGBA_8888_SkColorType, kUnpremul_SkAlphaType);
        const auto image = SkImages::RasterFromPixmapCopy(SkPixmap(info, source.pixels().data(), source.stride()));
        if (!image) throw std::runtime_error("could not import image");
        state->drawing().drawImage(image, origin.horizontal, origin.vertical);
    }

    Image Canvas::image() const {
        if (!state->surface) throw std::logic_error("a raster canvas is required");
        Image result(state->extent);
        if (!state->surface->readPixels(state->info(kUnpremul_SkAlphaType),
                                        result.pixels().data(), result.stride(), 0, 0))
            throw std::runtime_error("could not read canvas pixels");
        return result;
    }

    std::string Canvas::svg() {
        if (state->surface) throw std::logic_error("an SVG canvas is required");
        if (state->vector) {
            state->vector.reset();
            const auto data = state->stream.detachAsData();
            state->document.assign(static_cast<const char *>(data->data()), data->size());
        }
        return state->document;
    }
}
