#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace zero_shell {

struct Image {
    int width = 0;
    int height = 0;
    std::vector<uint8_t> rgba;

    bool empty() const { return width <= 0 || height <= 0 || rgba.empty(); }
};

bool load_png_image(const std::string &path, Image &image);

} // namespace zero_shell
