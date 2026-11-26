#include <complex>
#include <SDL3/SDL.h>

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>

#define WORLD_WIDTH 4
#define WORLD_HEIGHT 4
#define TILE_SIZE 128

SDL_Point tileCoords(int x, int y) {
    SDL_Point p;
    p.x = x / TILE_SIZE;
    p.y = y / TILE_SIZE;
    return p;
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

    bool world[WORLD_WIDTH][WORLD_HEIGHT]= {
        { 0, 1, 1, 0 },
        { 0, 0, 0, 0 },
        { 0, 0, 0, 0 },
        { 0, 0, 0, 0 }
    };

    uint32_t currentX = 250;
    uint32_t currentY = 250;
    int currentDirX = -512;
    int currentDirY = 1;

    bool running = true;
    uint64_t t = 0;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (event.type == SDL_EVENT_QUIT)
                running = false;
        }

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

        for (int x = 0; x < WORLD_WIDTH; x++) {
            for (int y = 0; y < WORLD_HEIGHT; y++) {
                if (world[x][y]) {
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
        rect.x = currentX - 4;
        rect.y = currentY - 4;
        rect.w = 8;
        rect.h = 8;
        SDL_RenderRect(renderer, &rect);
        SDL_RenderLine(renderer, currentX, currentY, currentX + currentDirX * 128, currentY + currentDirY * 128);

        SDL_Point tileCoord = tileCoords(currentX, currentY);
        float dtX = (tileCoord.x * TILE_SIZE + currentX) / currentDirX;
        float dtY = (tileCoord.y * TILE_SIZE + currentY) / currentDirY;

        SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
        SDL_FRect tilrect;
        tilrect.x = currentX + dtX - 4;
        tilrect.y = currentY + dtY - 4;
        tilrect.w = 8;
        tilrect.h = 8;
        SDL_RenderRect(renderer, &tilrect);

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
