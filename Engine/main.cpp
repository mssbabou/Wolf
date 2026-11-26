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

#define WORLD_WIDTH 4
#define WORLD_HEIGHT 4
#define TILE_SIZE 128

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

struct Hit { int cx, cy; float t; bool hitVerticalWall; };

std::optional<Hit> raycast(Vec2 o, Vec2 d, const Grid& g, float maxDist) {
    int cx = (int)std::floor(o.x), cy = (int)std::floor(o.y);
    int stepX = d.x > 0 ? 1 : -1;
    int stepY = d.y > 0 ? 1 : -1;

    float tDeltaX = d.x != 0 ? std::abs(1 / d.x) : INFINITY;
    float tDeltaY = d.y != 0 ? std::abs(1 / d.y) : INFINITY;

    float tMaxX = (d.x > 0 ? (cx + 1 - o.x) : (o.x - cx)) * tDeltaX;
    float tMaxY = (d.y > 0 ? (cy + 1 - o.y) : (o.y - cy)) * tDeltaY;

    float t = 0;
    while (t <= maxDist) {
        bool vertical;
        if (tMaxX < tMaxY) { cx += stepX; t = tMaxX; tMaxX += tDeltaX; vertical = true; }
        else { cy += stepY; t = tMaxY; tMaxY += tDeltaY; vertical = false; }

        if (!g.inBounds(cx, cy)) return std::nullopt;
        if (g.filled(cx, cy))    return Hit{ cx, cy, t, vertical };
    }
    return std::nullopt;
}

int main() {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("Ulvesteen", 1280, 720, SDL_WINDOW_HIGH_PIXEL_DENSITY);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
    if (!window || !renderer) {
        SDL_Log("Window/renderer failed: %s", SDL_GetError());
        return 1;
    }
    SDL_SetRenderVSync(renderer, 1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_DockingEnable;
    ImGui::StyleColorsDark();

    ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer3_Init(renderer);

    Grid grid(WORLD_WIDTH, WORLD_HEIGHT);
    grid.at(1, 0) = 1;
    grid.at(2, 0) = 1;

    Vec2 currentPos{ 1.8f, 1.8f };
    float currentRotation = 0;

    float moveSpeed = 1.0f;
    float turnSpeedDeg = 50.0f;

    float horizontalFOV = 90.0f;

    Input input;

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

        if (input.held(SDL_SCANCODE_W)) currentPos.y -= moveSpeed * dt;
        if (input.held(SDL_SCANCODE_A)) currentPos.x -= moveSpeed * dt;
        if (input.held(SDL_SCANCODE_S)) currentPos.y += moveSpeed * dt;
        if (input.held(SDL_SCANCODE_D)) currentPos.x += moveSpeed * dt;

        if (input.held(SDL_SCANCODE_RIGHT)) currentRotation += turnSpeedDeg * dt;
        if (input.held(SDL_SCANCODE_LEFT)) currentRotation -= turnSpeedDeg * dt;

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
        
        float angleIncrement = horizontalFOV / 1280;
        for (size_t i = 0; i < 1280; i++)
        {
            float angle = (currentRotation - (horizontalFOV / 2)) + angleIncrement * i;
            Vec2 direction = Vec2::fromAngle(degToRad(angle));

            if (auto hit = raycast(currentPos, direction, grid, 50.0f)) 
            {
                // Temporay size falloff function TO BE REPLACED
                int sizePixels = static_cast<int>(720.0f / hit->t);

                SDL_SetRenderDrawColor(renderer, 0, 0, 255 / hit->t, 255);
                SDL_RenderLine(renderer, i, (720 - sizePixels) / 2, i, ((720 - sizePixels) / 2) + sizePixels);
            }
        }

/*
        for (int x = 0; x < WORLD_WIDTH; x++) {
            for (int y = 0; y < WORLD_HEIGHT; y++) {
                if (grid.at(x, y) != 0) {
                    SDL_SetRenderDrawColor(renderer, 255, 100, 100, 255);
                }
                else {
                    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
                }

                SDL_FRect rect;
                rect.x = static_cast<float>(x) * TILE_SIZE + 1 * static_cast<float>(x);
                rect.y = static_cast<float>(y) * TILE_SIZE + 1 * static_cast<float>(y);
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

        if (auto hit = raycast(currentPos, currentDir, grid, 50.0f)) {
            SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
            SDL_FRect tilrect;
            tilrect.x = (currentPos.x + currentDir.x * hit->t) * TILE_SIZE - 4;
            tilrect.y = (currentPos.y + currentDir.y * hit->t) * TILE_SIZE - 4;
            tilrect.w = 8;
            tilrect.h = 8;
            float a = hit->t;
            SDL_RenderRect(renderer, &tilrect);
        }
*/

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
