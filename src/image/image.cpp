#include "image/image.hpp"

#include "include/core/SkData.h"
#include "include/core/SkImageInfo.h"
#include "include/core/SkPixmap.h"
#include "include/encode/SkPngEncoder.h"

#include <limits>
#include <stdexcept>

namespace gfx {
    Image::Image(Extent size) : extent(size) {
        if (size.width == 0 || size.height == 0 ||
            size.width > std::numeric_limits<int>::max() ||
            size.height > std::numeric_limits<int>::max() ||
            size.width > storage.max_size() / 4 / size.height)
            throw std::invalid_argument("invalid image dimensions");

        storage.resize(stride() * size.height);
    }

    std::vector<std::uint8_t> Image::png() const {
        const auto info = SkImageInfo::Make(static_cast<int>(extent.width), static_cast<int>(extent.height),
                                            kRGBA_8888_SkColorType, kUnpremul_SkAlphaType);
        const auto data = SkPngEncoder::Encode(SkPixmap(info, storage.data(), stride()), {});
        if (!data) throw std::runtime_error("could not encode PNG");
        const auto *const bytes = static_cast<const std::uint8_t *>(data->data());
        return {bytes, bytes + data->size()};
    }
}
