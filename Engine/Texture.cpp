#include "Texture.h"

#include <algorithm>
#include <cmath>
#include <SDL3/SDL_log.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#include <stb_image.h>

std::optional<Texture> Texture::load(const char* path) {
    int w, h, channels;
    unsigned char* data = stbi_load(path, &w, &h, &channels, 4);
    if (!data) {
        SDL_Log("Failed to load texture '%s': %s", path, stbi_failure_reason());
        return std::nullopt;
    }

    Texture tex;
    tex.width = w;
    tex.height = h;
    tex.pixels.resize(size_t(w) * h);
    for (size_t i = 0; i < tex.pixels.size(); i++) {
        const unsigned char* p = data + i * 4;
        tex.pixels[i] = Color{ p[0], p[1], p[2], p[3] };
    }

    stbi_image_free(data);
    return tex;
}

Color Texture::sample(float u, float v) const {
    u -= std::floor(u);
    v -= std::floor(v);
    int x = std::min(int(u * width), width - 1);
    int y = std::min(int(v * height), height - 1);
    return at(x, y);
}
