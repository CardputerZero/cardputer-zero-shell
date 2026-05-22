#include "zero_shell/image.hpp"

#include <cstdio>
#include <png.h>

namespace zero_shell {

bool load_png_image(const std::string &path, Image &image)
{
    image = {};

    FILE *fp = std::fopen(path.c_str(), "rb");
    if (!fp) {
        return false;
    }

    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if (!png) {
        std::fclose(fp);
        return false;
    }

    png_infop info = png_create_info_struct(png);
    if (!info) {
        png_destroy_read_struct(&png, nullptr, nullptr);
        std::fclose(fp);
        return false;
    }

    if (setjmp(png_jmpbuf(png))) {
        png_destroy_read_struct(&png, &info, nullptr);
        std::fclose(fp);
        image = {};
        return false;
    }

    png_init_io(png, fp);
    png_read_info(png, info);

    png_uint_32 width = png_get_image_width(png, info);
    png_uint_32 height = png_get_image_height(png, info);
    png_byte color_type = png_get_color_type(png, info);
    png_byte bit_depth = png_get_bit_depth(png, info);

    if (bit_depth == 16) {
        png_set_strip_16(png);
    }
    if (color_type == PNG_COLOR_TYPE_PALETTE) {
        png_set_palette_to_rgb(png);
    }
    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) {
        png_set_expand_gray_1_2_4_to_8(png);
    }
    if (png_get_valid(png, info, PNG_INFO_tRNS)) {
        png_set_tRNS_to_alpha(png);
    }
    if (color_type == PNG_COLOR_TYPE_RGB ||
        color_type == PNG_COLOR_TYPE_GRAY ||
        color_type == PNG_COLOR_TYPE_PALETTE) {
        png_set_filler(png, 0xFF, PNG_FILLER_AFTER);
    }
    if (color_type == PNG_COLOR_TYPE_GRAY ||
        color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
        png_set_gray_to_rgb(png);
    }

    png_read_update_info(png, info);

    image.width = static_cast<int>(width);
    image.height = static_cast<int>(height);
    image.rgba.resize(static_cast<size_t>(image.width) * static_cast<size_t>(image.height) * 4);

    std::vector<png_bytep> rows(static_cast<size_t>(image.height));
    for (int y = 0; y < image.height; ++y) {
        rows[static_cast<size_t>(y)] =
            image.rgba.data() + static_cast<size_t>(y) * static_cast<size_t>(image.width) * 4;
    }

    png_read_image(png, rows.data());
    png_read_end(png, nullptr);
    png_destroy_read_struct(&png, &info, nullptr);
    std::fclose(fp);
    return true;
}

} // namespace zero_shell
