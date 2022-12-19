#include <stdio.h>
#include <assert.h>

#include "debug.h"
#include "fs.h"
#include "heap.h"
#include "render.h"
#include "frogger_game.h"
#include "timer.h"
#include "wm.h"
#include "imguiWindow.h"
#include "audio.h"

typedef struct imgui_info_t
{
    SDL_Window* window;
    ImGui_ImplVulkanH_Window* wd;
    ImVec4 clearColor;

    // Program specific data
    bool orthoView;
    bool perspView;
    float viewDistance;
    float horizontalPan;
    float verticalPan;
    float viewDistanceP;
    float horizontalPanP;
    float verticalPanP;
    float yaw;
    float pitch;
    float roll;

    bool bgmusic;
    int bgmVolume;
    int difficulty;
    float playerSpeed;
    ImVec4 playerColor;

    bool audioChange;
    bool update;
    bool quit;
} imgui_info_t;

typedef struct engine_info_t
{
    bool orthoView;
    float viewDistance;
    float horizontalPan;
    float verticalPan;
    float viewDistanceP;
    float horizontalPanP;
    float verticalPanP;
    float yaw;
    float pitch;
    float roll;

    int difficulty;
    float playerSpeed;
    ImVec4 playerColor;
} engine_info_t;

// May seem unnecessary currently
// Needed if data type become complicated
void dataTransfer(imgui_info_t* imgui_info, engine_info_t* engine_info)
{
    engine_info->orthoView = imgui_info->orthoView;
    engine_info->viewDistance = imgui_info->viewDistance;
    engine_info->viewDistanceP = imgui_info->viewDistanceP;
    engine_info->horizontalPan = imgui_info->horizontalPan;
    engine_info->horizontalPanP = imgui_info->horizontalPanP;
    engine_info->verticalPan = imgui_info->verticalPan;
    engine_info->verticalPanP = imgui_info->verticalPanP;
    engine_info->yaw = imgui_info->yaw;
    engine_info->pitch = imgui_info->pitch;
    engine_info->roll = imgui_info->roll;
    engine_info->difficulty = imgui_info->difficulty;
    engine_info->playerSpeed = imgui_info->playerSpeed;
    engine_info->playerColor = imgui_info->playerColor;
}

int main(int argc, const char* argv[])
{
    debug_set_print_mask(k_print_info | k_print_warning | k_print_error);
    debug_install_exception_handler();

    timer_startup();

    heap_t* heap = heap_create(2 * 1024 * 1024);
    fs_t* fs = fs_create(heap, 8);
    wm_window_t* window = wm_create(heap);
    render_t* render = render_create(heap, window);
    imgui_info_t* imgui_info = SetUpImgui(heap);

    frogger_game_t* game = frogger_game_create(heap, fs, window, render, 2);
    engine_info_t* engine_info = heap_alloc(heap, sizeof(engine_info_t), 8);

    if (SDL_Init(SDL_INIT_AUDIO) < 0)
    {
        printf("Audio Device Initialization Failed!\n");
        return 1;
    }
    initAudio();

    playMusic("music/main_theme.wav", SDL_MIX_MAXVOLUME);

    // The while loop handles two windows
    // Tried to put everything in onw window but imgui_impl_win32.cpp and the generated cimgui_impl.c does not work
    while (!wm_pump(window))
    {
        // Quit the program
        if (imgui_info->quit)
            break;

        // Update and restart the game
        if (imgui_info->update) 
        {
            printf("GAME UPDATE!\n");
            imgui_info->update = false;
            frogger_game_destroy(game);
            game = frogger_game_create(heap, fs, window, render, imgui_info->difficulty);
        }

        // Audio Control
        if (!imgui_info->bgmusic)
        {
            pauseAudio();
        }
        else 
        {
            unpauseAudio();
        }

        if (imgui_info->audioChange)
        {
            playMusic("music/main_theme.wav", imgui_info->bgmVolume);
            imgui_info->audioChange = false;
        }

        DrawImgui(imgui_info);
        frogger_game_update(game, engine_info);
        dataTransfer(imgui_info, engine_info);
    }

    endAudio();
    SDL_Quit();

    /* XXX: Shutdown render before the game. Render uses game resources. */
    render_destroy(render);

    frogger_game_destroy(game);
    DestoryImgui(imgui_info);

    wm_destroy(window);
    fs_destroy(fs);
    heap_destroy(heap);
    
    return 0;
}
