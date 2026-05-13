//
// Created by user on 4/27/2026.
//
#include "config.h"

void config_load_defaults(AppConfig *config) {
    if (config == NULL) {
        return;
    }

    config->canalLength = 10;
    config->tickMs = 500;

    config->leftInitialShips = 4;
    config->rightInitialShips = 4;

    config->schedulerType = SCHED_FCFS;
    config->flowType = FLOW_SIGN;

    config->fairnessW = 2;
    config->signInterval = 4;
    config->rrQuantum = 2;

    config->demoMaxTicks = 40;
}