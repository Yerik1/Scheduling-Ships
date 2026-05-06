#include <stdio.h>
#include "ui_state.h"

void SaveConfiguration(UIState *state)
{
    FILE *file = fopen("simulation.config", "w");
    if (file == NULL)
        return;

    const char *sched_names[] = {"FCFS", "ROUND_ROBIN", "PRIORITY", "SJF", "STRN", "EDF"};
    const char *flow_names[] = {"EQUITY_W", "SIGN", "TICO"};

    // Header
    fprintf(file, "# SYSTEM CANAL CONFIG\n");
    fprintf(file, "CANAL_LEN=%d\n", state->config.canal_len);
    fprintf(file, "SHIP_SPEED=%d\n", state->config.ship_speed);

    // Left Ships
    fprintf(file, "LEFT_FISHING=%d\n", state->config.ships_left[0]);
    fprintf(file, "LEFT_NORMAL=%d\n", state->config.ships_left[1]);
    fprintf(file, "LEFT_PATROL=%d\n", state->config.ships_left[2]);

    // Right Ships
    fprintf(file, "RIGHT_FISHING=%d\n", state->config.ships_right[0]);
    fprintf(file, "RIGHT_NORMAL=%d\n", state->config.ships_right[1]);
    fprintf(file, "RIGHT_PATROL=%d\n", state->config.ships_right[2]);

    // --- Scheduler Transcription ---
    int schedIdx = state->config.drop_sched.selectedIndex;
    fprintf(file, "SCHEDULER_NAME=%s\n", sched_names[schedIdx]);

    if (schedIdx == 1) // Round Robin
    {
        fprintf(file, "RR_QUANTUM=%d\n", state->config.rr_quantum);
    }

    // --- Flow Transcription ---
    int flowIdx = state->config.drop_flow.selectedIndex;
    fprintf(file, "FLOW_NAME=%s\n", flow_names[flowIdx]);

    if (flowIdx == 0) // Equity
    {
        fprintf(file, "FAIRNESS_W=%d\n", state->config.fairness_w);
    }
    else if (flowIdx == 1) // Sign
    {
        fprintf(file, "SIGN_INTERVAL=%d\n", state->config.sign_interval);
    }

    fclose(file);
}
