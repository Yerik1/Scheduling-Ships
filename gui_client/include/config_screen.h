#ifndef CONFIG_SCREEN_H
#define CONFIG_SCREEN_H

#include "raylib.h"
#include "ui_state.h"
#include "components.h"

const char *config_params[] = {
    {"CANAL LENGTH"},
    {"LEFT FISHING SHIPS"},
    {"LEFT NORMAL SHIPS"},
    {"LEFT PATROL SHIPS"},
    {"RIGHT FISHING SHIPS"},
    {"RIGHT NORMAL SHIPS"},
    {"RIGHT PATROL SHIPS"},
    {"SHIP SPEED"},
    {"SCHEDULER TYPE"},
    {"FLOW TYPE"}};

const char *config_rr_params[] = {
    "QUATUM (ms)"};

const char *config_equity_params[] = {
    "FAIRNESS (W)"};

const char *config_sign_params[] = {
    "SIGN INVERVAL (s)"};

// typedef enum
// {
//     FCSC,
//     RR,
//     PRIORITY,
//     SFJ,
//     STRN,
//     EDF
// } SCHEDULER;

// typedef enum
// {
//     EQUITY,
//     SIGN,
//     TICO
// } FLOW;

const int num_of_options = sizeof(config_params) / sizeof(config_params[0]);

void ProcessConfigScreen(UIState *state);
void DrawConfigScreen(UIState *state);

#endif
