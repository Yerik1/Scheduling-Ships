#include "ui_state.h"
#include <string.h>

static const char *scheduler_options[] = {"FCFS", "ROUND ROBIN", "PRIORITY", "SJF", "STRN", "EDF"};
static const char *flow_options[] = {"EQUIDAD (W)", "LETRERO", "TICO"};
static const char *ship_labels[] = {"FISHING:", "NORMAL:", "PATROL:"};

void InitUI(UIState *state)
{
    // 1. Get Screen Dimensions
    int sw = GetScreenWidth();
    int sh = GetScreenHeight();

    // 2. Set Initial State
    state->current_screen = SCREEN_CONFIG;
    state->running = true;
    state->config.selected_option = 0;

    // 3. Layout Constants
    float boxW = 220;
    float boxH = 35;
    float centerX = sw / 2.0f - (boxW / 2.0f);
    float leftCol = sw * 0.25f - (boxW / 2.0f);
    float rightCol = sw * 0.75f - (boxW / 2.0f);
    float dropY = sh * 0.62f;
    float extraY = sh * 0.72f;

    // --- SECTION: CANAL ---
    state->config.txt_len = (TextBox){
        .bounds = {centerX, sh * 0.15f, boxW, boxH},
        .label = "CANAL LEN (m):",
        .buffer = "100",
        .cursor_pos = 3};

    // --- SECTION: SHIPS ---
    for (int i = 0; i < 3; i++)
    {
        float y = sh * 0.25f + (i * 75);

        // Left
        state->config.left_ships[i].bounds = (Rectangle){leftCol, y, boxW, boxH};
        strcpy(state->config.left_ships[i].label, ship_labels[i]);
        strcpy(state->config.left_ships[i].buffer, "0");

        // Right
        state->config.right_ships[i].bounds = (Rectangle){rightCol, y, boxW, boxH};
        strcpy(state->config.right_ships[i].label, ship_labels[i]);
        strcpy(state->config.right_ships[i].buffer, "0");
    }

    // --- SECTION: SPEED ---
    state->config.txt_speed = (TextBox){
        .bounds = {centerX, sh * 0.50f, boxW, boxH},
        .label = "SHIP SPEED (m/s):",
        .buffer = "5",
        .cursor_pos = 1};

    // --- SECTION: SCHEDULER (Dropdown) ---
    state->config.drop_sched = (Dropdown){
        .bounds = {leftCol, dropY, boxW + 40, boxH + 5},
        .options = scheduler_options,
        .optionsCount = 6,
        .selectedIndex = 0,
        .isExpanded = false};
    strcpy(state->config.drop_sched.label, "SCHEDULER TYPE");

    // --- SECTION: FLOW (Dropdown) ---
    state->config.drop_flow = (Dropdown){
        .bounds = {rightCol, dropY, boxW + 40, boxH + 5},
        .options = flow_options,
        .optionsCount = 3,
        .selectedIndex = 0,
        .isExpanded = false};
    strcpy(state->config.drop_flow.label, "FLOW TYPE");

    // --- SECTION: EXTRA PARAMS ---
    state->config.txt_quantum = (TextBox){
        .bounds = {sw * 0.25f - boxW / 2, extraY, boxW, boxH},
        .label = "QUANTUM (ms):",
        .buffer = "10",
        .cursor_pos = 2};

    state->config.txt_fairness_w = (TextBox){
        .bounds = {sw * 0.50f - boxW / 2, extraY, boxW, boxH},
        .label = "FAIRNESS W:",
        .buffer = "5",
        .cursor_pos = 1};

    state->config.txt_sign_interval = (TextBox){
        .bounds = {sw * 0.75f - boxW / 2, extraY, boxW, boxH},
        .label = "SIGN INTERVAL (s):",
        .buffer = "30",
        .cursor_pos = 2};
}
