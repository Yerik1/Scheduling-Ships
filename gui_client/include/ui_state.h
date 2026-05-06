#ifndef UI_STATE_H
#define UI_STATE_H

#include "raylib.h"
#include "components.h"

typedef enum
{
    SCREEN_CONFIG,
    SCREEN_SIMULATION
} AppScreen;

typedef struct
{
    AppScreen current_screen;
    bool running;
    struct
    {
        TextBox txt_len;
        TextBox left_ships[3];
        TextBox right_ships[3];
        TextBox txt_speed;

        Dropdown drop_sched;
        int selected_scheduler;
        Dropdown drop_flow;
        int selected_flow;

        TextBox txt_quantum;
        TextBox txt_fairness_w;
        TextBox txt_sign_interval;
        bool needs_extra;

        int canal_len;
        int ships_left[3];  // [0]: Pesquero, [1]: Normal, [2]: Patrulla
        int ships_right[3]; // [0]: Pesquero, [1]: Normal, [2]: Patrulla
        int ship_speed;
        int rr_quantum;
        int fairness_w;
        int sign_interval;

        int selected_option;
    } config;
} UIState;

void InitUI(UIState *state);

#endif
