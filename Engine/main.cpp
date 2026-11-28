#include <complex>
#include <cmath>
#include <array>

#include <SDL3/SDL.h>

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>
#include <optional>
#include <vector>
#include <numbers>

#include "Texture.h"

#define WORLD_WIDTH 16
#define WORLD_HEIGHT 16
#define TILE_SIZE 16

constexpr int screenWidth = 1280;
constexpr int screenHeight = 720;

struct Input {
    std::array<bool, SDL_SCANCODE_COUNT> prev{}, curr{};

    void update() {
        prev = curr;
        const bool* keys = SDL_GetKeyboardState(nullptr);
        std::copy(keys, keys + SDL_SCANCODE_COUNT, curr.begin());
    }

    bool held(SDL_Scancode k)     const { return curr[k]; }
    bool pressed(SDL_Scancode k)  const { return curr[k] && !prev[k]; }
    bool released(SDL_Scancode k) const { return !curr[k] && prev[k]; }
};

constexpr float degToRad(float deg) { return deg * (std::numbers::pi_v<float> / 180.0f); }
constexpr float radToDeg(float rad) { return rad * (180.0f / std::numbers::pi_v<float>); }

struct Vec2 {
    float x = 0, y = 0;

    Vec2 operator+(Vec2 o) const { return { x + o.x, y + o.y }; }
    Vec2 operator-(Vec2 o) const { return { x - o.x, y - o.y }; }
    Vec2 operator*(float s) const { return { x * s, y * s }; }
    Vec2 operator/(float s) const { return { x / s, y / s }; }
    Vec2& operator+=(Vec2 o) { x += o.x; y += o.y; return *this; }
    Vec2& operator-=(Vec2 o) { x -= o.x; y -= o.y; return *this; }

    float dot(Vec2 o) const { return x * o.x + y * o.y; }
    float cross(Vec2 o) const { return x * o.y - y * o.x; }
    float magnitude() const { return std::sqrt(dot(*this)); }
    float sqrMagnitude() const { return (dot(*this)); };
    Vec2 normalized() const { float l = magnitude(); return l > 0 ? *this / l : *this; }
    float angleTo(Vec2 o) const {
        return std::atan2(cross(o), dot(o));
    }
    Vec2 rotated(float rad) const {
        float c = std::cos(rad), s = std::sin(rad);
        return { x * c - y * s, x * s + y * c };
    }
    static Vec2 fromAngle(float rad) {
        return { std::sin(rad), -std::cos(rad) };
    }

    operator SDL_FPoint() const { return { x, y }; }
};

inline Vec2 operator*(float s, Vec2 v) { return v * s; }

static Vec2 WishDir(Vec2 input, Vec2 forward) {
    forward = forward.normalized();
    Vec2 right = Vec2(forward.y, -forward.x);

    Vec2 wishDir = forward * input.y + right * input.x;
    return wishDir.sqrMagnitude() > 0.0f ? wishDir.normalized() : Vec2(0, 0);
}

static Vec2 Accelerate(Vec2 velocity, Vec2 wishDir, float wishSpeed, float accel, float dt) {
    float currentSpeed = velocity.dot(wishDir);
    float addSpeed = wishSpeed - currentSpeed;

    if (addSpeed <= 0.0f) return velocity;

    float accelSpeed = std::min(accel * wishSpeed * dt, addSpeed);
    return velocity + wishDir * accelSpeed;
}

static Vec2 ApplyFriction(Vec2 velocity, float friction, float stopSpeed, float dt) {
    float speed = velocity.magnitude();
    if (speed < 0.0001f) return Vec2(0, 0);

    float control = std::max(speed, stopSpeed);
    float newSpeed = std::max(speed - control * friction * dt, 0.0f);
    return velocity * (newSpeed / speed);
}

struct Grid {
    int width = 0, height = 0;
    std::vector<uint8_t> tiles;

    Grid(int w, int h) : width(w), height(h), tiles(w* h, 0) {}

    bool inBounds(int x, int y) const {
        return x >= 0 && y >= 0 && x < width && y < height;
    }

    uint8_t& at(int x, int y) { return tiles[y * width + x]; }
    uint8_t  at(int x, int y) const { return tiles[y * width + x]; }

    bool filled(int x, int y) const { return at(x, y) != 0; }
};

struct GridHit { int cx, cy; float d; bool hitVerticalWall; float t; };
struct BillboardHit { int i; float d; float t; };

std::optional<GridHit> raycastGrid(Vec2 origin, Vec2 direction, const Grid& g, float maxDist) {
    int cx = (int)std::floor(origin.x), cy = (int)std::floor(origin.y);
    int stepX = direction.x > 0 ? 1 : -1;
    int stepY = direction.y > 0 ? 1 : -1;

    float tDeltaX = direction.x != 0 ? std::abs(1 / direction.x) : INFINITY;
    float tDeltaY = direction.y != 0 ? std::abs(1 / direction.y) : INFINITY;

    float tMaxX = (direction.x > 0 ? (cx + 1 - origin.x) : (origin.x - cx)) * tDeltaX;
    float tMaxY = (direction.y > 0 ? (cy + 1 - origin.y) : (origin.y - cy)) * tDeltaY;

    float d = 0;
    while (d <= maxDist) {
        bool vertical;
        if (tMaxX < tMaxY) { cx += stepX; d = tMaxX; tMaxX += tDeltaX; vertical = true; }
        else { cy += stepY; d = tMaxY; tMaxY += tDeltaY; vertical = false; }

        if (!g.inBounds(cx, cy)) return std::nullopt;
        if (g.filled(cx, cy)) {
            Vec2 p = origin + direction * d;
            float t = vertical ? p.y - std::floor(p.y) : p.x - std::floor(p.x);
            if ((vertical && stepX < 0) || (!vertical && stepY > 0)) t = 1.0f - t;
            return GridHit{ cx, cy, d, vertical, t };
        }
    }
    return std::nullopt;
}

int main() {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("Ulvesteen", screenWidth, screenHeight, SDL_WINDOW_HIGH_PIXEL_DENSITY);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
    if (!window || !renderer) {
        SDL_Log("Window/renderer failed: %s", SDL_GetError());
        return 1;
    }

    SDL_Texture* tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_STREAMING, screenWidth, screenHeight);
    SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_NEAREST);

    SDL_SetRenderVSync(renderer, 1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_DockingEnable;
    ImGui::StyleColorsDark();

    ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer3_Init(renderer);

    Grid grid(WORLD_WIDTH, WORLD_HEIGHT);
    grid.at(7, 7) = 1;
    grid.at(8, 7) = 2;

    Vec2 currentPos = Vec2(8.8f, 10.8f);
    Vec2 currentVelocity;
    float currentRotation = 0;
    Vec2 currentDir;

    float moveSpeed = 1.0f;
    float turnSpeedDeg = 100.0f;

    float horizontalFOV = 90.0f;

    Input input;

    Texture textures[16];

    textures[0] = *Texture::load("basalt.jpg");
    textures[1] = *Texture::load("gecko.jpg");

    uint32_t* pixelBuffer = (uint32_t*)malloc(screenWidth * screenHeight * sizeof(uint32_t));

    Vec2 billboardPos = Vec2(8.0f, 8.0f);
    float billboardWidth = 1.0f;

    bool running = true;
    uint64_t t = 0;
    Uint64 last = SDL_GetTicksNS();
    while (running) {
        Uint64 now = SDL_GetTicksNS();
        float dt = (now - last) / 1e9f;
        last = now;

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (event.type == SDL_EVENT_QUIT)
                running = false;
        }
        input.update();

        if (input.held(SDL_SCANCODE_RIGHT)) currentRotation += turnSpeedDeg * dt;
        if (input.held(SDL_SCANCODE_LEFT)) currentRotation -= turnSpeedDeg * dt;

        currentDir = Vec2::fromAngle(degToRad(currentRotation));

        Vec2 wishDir;
        if (input.held(SDL_SCANCODE_W)) wishDir.y += 1;
        if (input.held(SDL_SCANCODE_A)) wishDir.x += 1;
        if (input.held(SDL_SCANCODE_S)) wishDir.y -= 1;
        if (input.held(SDL_SCANCODE_D)) wishDir.x -= 1;
        wishDir = WishDir(wishDir, currentDir);
        currentVelocity = ApplyFriction(currentVelocity, 6.0f, 1.0f, dt);
        currentVelocity = Accelerate(currentVelocity, wishDir, 2.0f, 10.0f, dt);
        currentPos += currentVelocity * dt;


        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("Performance");
        ImGui::Text("%.1f FPS", io.Framerate);
        ImGui::End();

        ImGui::Render();
        SDL_SetRenderScale(renderer, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
        SDL_SetRenderDrawColor(renderer, 30, 30, 36, 255);
        SDL_RenderClear(renderer);

        memset(pixelBuffer, 0, screenWidth * screenHeight * sizeof(uint32_t));
        
        float halfFovTan = std::tan(degToRad(horizontalFOV / 2));
        float focal = (screenWidth / 2.0f) / halfFovTan;
        for (size_t c = 0; c < screenWidth; c++)
        {
            float screenX = 2.0f * (c + 0.5f) / screenWidth - 1.0f;
            float angle = currentRotation + radToDeg(std::atan(screenX * halfFovTan));
            Vec2 direction = Vec2::fromAngle(degToRad(angle));

            if (auto hit = raycastGrid(currentPos, direction, grid, 50.0f))
            {
                float perpDist = hit->d * std::cos(degToRad(angle - currentRotation));
                int sizePixels = static_cast<int>(focal / perpDist);
                float sizePixelsFloat = static_cast<float>(sizePixels);
                float distanceFalloff = 1 / hit->d;
                int start = (screenHeight - sizePixels) / 2;
                int pBegin = std::max(0, -start);
                int pEnd = std::min(sizePixels, screenHeight - start);
                Texture& texture = textures[grid.at(hit->cx, hit->cy) - 1];

                for (int p = pBegin; p < pEnd; p++)
                {
                    int y = start + p;
                    Vec2 texel{ hit->t, static_cast<float>(p) / sizePixelsFloat };
                    Color textureColorSample = texture.sample(texel.x, texel.y) * distanceFalloff;
                    pixelBuffer[y * screenWidth + c] = textureColorSample.packed();
                }
            }
        }

        SDL_UpdateTexture(tex, NULL, pixelBuffer, screenWidth * sizeof(uint32_t));
        SDL_RenderTexture(renderer, tex, NULL, NULL);

        for (int x = 0; x < WORLD_WIDTH; x++) {
            for (int y = 0; y < WORLD_HEIGHT; y++) {
                if (grid.at(x, y) != 0) {
                    SDL_SetRenderDrawColor(renderer, 255, 100, 100, 255);
                }
                else {
                    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 50);
                }

                SDL_FRect rect;
                rect.x = static_cast<float>(x) * TILE_SIZE;
                rect.y = static_cast<float>(y) * TILE_SIZE;
                rect.w = TILE_SIZE;
                rect.h = TILE_SIZE;
                SDL_RenderFillRect(renderer, &rect);
            }
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 255, 255);
        SDL_FRect rect;
        rect.x = currentPos.x * TILE_SIZE - 4;
        rect.y = currentPos.y * TILE_SIZE - 4;
        rect.w = 8;
        rect.h = 8;
        SDL_RenderRect(renderer, &rect);
        SDL_RenderLine(renderer, currentPos.x * TILE_SIZE, currentPos.y * TILE_SIZE, currentPos.x * TILE_SIZE + currentDir.x * 128, currentPos.y * TILE_SIZE + currentDir.y * 128);

        if (auto hit = raycastGrid(currentPos, currentDir, grid, 50.0f)) {
            SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
            SDL_FRect tilrect;
            tilrect.x = (currentPos.x + currentDir.x * hit->d) * TILE_SIZE - 4;
            tilrect.y = (currentPos.y + currentDir.y * hit->d) * TILE_SIZE - 4;
            tilrect.w = 8;
            tilrect.h = 8;
            float a = hit->d;
            SDL_RenderRect(renderer, &tilrect);
        }

        float r = degToRad(currentRotation);

        Vec2 forward{ std::sin(r), -std::cos(r) };
        Vec2 right{ std::cos(r),  std::sin(r) };

        Vec2 d{ billboardPos.x - currentPos.x, billboardPos.y - currentPos.y };

        float depth = d.x * forward.x + d.y * forward.y;
        float side = d.x * right.x + d.y * right.y;

        if (depth > 0.01f) {
            const float focal = (screenWidth / 2.0f) / std::tan(degToRad(horizontalFOV) / 2.0f);

            float screenX = (screenWidth / 2.0f) + (side / depth) * focal;
            float screenY = (screenHeight / 2.0f);
            float size = (billboardWidth / depth) * focal;

            SDL_FRect billrect;
            billrect.x = screenX - size / 2;
            billrect.y = screenY - size / 2;
            billrect.w = size;
            billrect.h = size;

            SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);
            SDL_RenderRect(renderer, &billrect);
        }

        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
        SDL_RenderPresent(renderer);

        t++;
    }

    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
