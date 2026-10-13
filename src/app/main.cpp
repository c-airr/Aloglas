// Placeholder entry point: proves SDL3 links and runs. Replaced by the real app.
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

int main(int, char**) {
    if (!SDL_Init(0)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }
    const int v = SDL_GetVersion();
    SDL_Log("SDL %d.%d.%d", SDL_VERSIONNUM_MAJOR(v), SDL_VERSIONNUM_MINOR(v), SDL_VERSIONNUM_MICRO(v));
    SDL_Quit();
    return 0;
}
