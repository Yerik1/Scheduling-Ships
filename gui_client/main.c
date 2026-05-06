#include "ui_state.h"
#include <string.h>

int main()
{
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(0, 0, "Scheduling Ships - Config");
    ToggleFullscreen();

    SetTargetFPS(60);

    UIState gui = {0};
    InitUI(&gui);

    while (gui.running && !WindowShouldClose())
    {
        if (IsKeyPressed(KEY_W))
            gui.running = false;

        BeginDrawing();
        ClearBackground(BLACK);
        switch (gui.current_screen)
        {
        case SCREEN_CONFIG:
            ProcessConfigScreen(&gui);
            DrawConfigScreen(&gui);
            break;

        case SCREEN_SIMULATION:
            DrawText("SIMULACIÓN EN CURSO...", 250, 300, 30, SKYBLUE);
            DrawText("Presione 'W' para salir", 10, 10, 20, GRAY);
            break;
        }
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
