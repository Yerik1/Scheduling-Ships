#include "config_screen.h"
#include "config_writer.h"

void ProcessConfigScreen(UIState *state)
{
    Vector2 mousePoint = GetMousePosition();

    // 2. Detect click
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        if (CheckCollisionPointRec(mousePoint, state->config.txt_len.bounds))
            state->config.selected_option = 0;

        for (int i = 0; i < 3; i++)
        {
            if (CheckCollisionPointRec(mousePoint, state->config.left_ships[i].bounds))
                state->config.selected_option = 1 + i;
            if (CheckCollisionPointRec(mousePoint, state->config.right_ships[i].bounds))
                state->config.selected_option = 4 + i;
        }

        if (CheckCollisionPointRec(mousePoint, state->config.txt_speed.bounds))
            state->config.selected_option = 7;

        if (CheckCollisionPointRec(mousePoint, state->config.drop_sched.bounds))
            state->config.selected_option = 8;
        if (CheckCollisionPointRec(mousePoint, state->config.drop_flow.bounds))
            state->config.selected_option = 9;

        if (CheckCollisionPointRec(mousePoint, state->config.txt_quantum.bounds))
            state->config.selected_option = 10;
        if (CheckCollisionPointRec(mousePoint, state->config.txt_fairness_w.bounds))
            state->config.selected_option = 11;
        if (CheckCollisionPointRec(mousePoint, state->config.txt_sign_interval.bounds))
            state->config.selected_option = 12;
    }

    // 2. Capture Keyboard Input
    state->config.txt_len.is_active = (state->config.selected_option == 0);
    state->config.txt_speed.is_active = (state->config.selected_option == 7);
    state->config.txt_quantum.is_active = (state->config.selected_option == 10);
    state->config.txt_fairness_w.is_active = (state->config.selected_option == 11);
    state->config.txt_sign_interval.is_active = (state->config.selected_option == 12);

    for (int i = 0; i < 3; i++)
    {
        state->config.left_ships[i].is_active = (state->config.selected_option == 1 + i);
        state->config.right_ships[i].is_active = (state->config.selected_option == 4 + i);

        if (state->config.left_ships[i].is_active)
            UpdateTextBox(&state->config.left_ships[i]);
        if (state->config.right_ships[i].is_active)
            UpdateTextBox(&state->config.right_ships[i]);
    }

    if (state->config.txt_len.is_active)
        UpdateTextBox(&state->config.txt_len);
    if (state->config.txt_speed.is_active)
        UpdateTextBox(&state->config.txt_speed);

    UpdateDropdown(&state->config.drop_sched, state->config.selected_option == 8);
    UpdateDropdown(&state->config.drop_flow, state->config.selected_option == 9);

    if (state->config.txt_quantum.is_active)
        UpdateTextBox(&state->config.txt_quantum);
    if (state->config.txt_fairness_w.is_active)
        UpdateTextBox(&state->config.txt_fairness_w);
    if (state->config.txt_sign_interval.is_active)
        UpdateTextBox(&state->config.txt_sign_interval);

    // 3. Confirm
    if (IsKeyPressed(KEY_ENTER) && !state->config.drop_sched.isExpanded && !state->config.drop_flow.isExpanded)
    {
        // Extra Params Flags
        bool isRR = state->config.selected_scheduler == 1;
        bool isEquity = state->config.selected_flow == 0;
        bool isSign = state->config.selected_flow == 1;

        // Convert Input to INT
        state->config.canal_len = atoi(state->config.txt_len.buffer);
        state->config.ship_speed = atoi(state->config.txt_speed.buffer);

        for (int i = 0; i < 3; i++)
        {
            state->config.ships_left[i] = atoi(state->config.left_ships[i].buffer);
            state->config.ships_right[i] = atoi(state->config.right_ships[i].buffer);
        }

        // Only Apply Atoi to Significant Params
        if (isRR)
            state->config.rr_quantum = atoi(state->config.txt_quantum.buffer);
        if (isEquity)
            state->config.fairness_w = atoi(state->config.txt_fairness_w.buffer);
        if (isSign)
            state->config.sign_interval = atoi(state->config.txt_sign_interval.buffer);

        // Generate .config
        SaveConfiguration(state);

        state->current_screen = SCREEN_SIMULATION;
    }
}

void DrawConfigScreen(UIState *state)
{
    const int sw = GetScreenWidth();
    const int sh = GetScreenHeight();

    // Title
    const char *title = "HARDWARE CONFIGURATION";
    int fontSize = 30;
    DrawText(title, (sw - MeasureText(title, fontSize)) / 2, sh * 0.05f, fontSize, RAYWHITE);

    // Canal Len Input
    DrawTextBox(&state->config.txt_len);

    // Ships Inputs
    DrawText("LEFT SIDE SHIPS", state->config.left_ships[0].bounds.x, sh * 0.18f, 20, SKYBLUE);
    DrawText("RIGHT SIDE SHIPS", state->config.right_ships[0].bounds.x, sh * 0.18f, 20, ORANGE);

    for (int i = 0; i < 3; i++)
    {
        DrawTextBox(&state->config.left_ships[i]);
        DrawTextBox(&state->config.right_ships[i]);
    }

    // Ships Speed Input
    DrawTextBox(&state->config.txt_speed);

    // Scheduler & Flow
    DrawDropdown(&state->config.drop_sched, state->config.selected_option == 8);
    DrawDropdown(&state->config.drop_flow, state->config.selected_option == 9);

    // Extra Params
    DrawTextBox(&state->config.txt_quantum);
    DrawTextBox(&state->config.txt_fairness_w);
    DrawTextBox(&state->config.txt_sign_interval);

    // Draw Expanded Dropdown at last
    if (state->config.drop_sched.isExpanded)
        DrawDropdown(&state->config.drop_sched, true);
    if (state->config.drop_flow.isExpanded)
        DrawDropdown(&state->config.drop_flow, true);

    const char *help = "Use MOUSE CURSOR to select & write the value | ENTER to start";
    DrawText(help, (sw - MeasureText(help, 15)) / 2, sh * 0.92f, 15, GRAY);
}
