#pragma once
#include <cstdint>
#include <optional>
#include <vector>

struct Color {
    uint8_t r = 0, g = 0, b = 0, a = 255;

    constexpr uint32_t packed() const {
        return (uint32_t(a) << 24) | (uint32_t(r) << 16) | (uint32_t(g) << 8) | uint32_t(b);
    }

    constexpr Color operator*(float s) const {
        auto f = [s](uint8_t c) {
            float v = c * s;
            return uint8_t(v < 0 ? 0 : v > 255 ? 255 : v);
        };
        return { f(r), f(g), f(b), a };
    }
    constexpr Color operator*(Color o) const {
        auto f = [](uint8_t x, uint8_t y) { return uint8_t((x * y + 127) / 255); };
        return { f(r, o.r), f(g, o.g), f(b, o.b), f(a, o.a) };
    }
    constexpr Color& operator*=(float s) { return *this = *this * s; }
    constexpr Color& operator*=(Color o) { return *this = *this * o; }
};

constexpr Color operator*(float s, Color c) { return c * s; }

struct Texture {
    int width = 0, height = 0;
    std::vector<Color> pixels;

    static std::optional<Texture> load(const char* path);

    Color at(int x, int y) const { return pixels[y * width + x]; }

    Color sample(float u, float v) const;
};
