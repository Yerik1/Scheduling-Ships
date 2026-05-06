//
// Created by user on 4/27/2026.
//

#ifndef CONFIG_H
#define CONFIG_H

#include "../flow_policies/flow_policy.h"

typedef enum {
    SCHED_FCFS,
    SCHD_RR,
    SCHED_PRIORITY,
    SCHED_SJF,
    SCHED_STRN,
    SCHED_EDF
} SchedulerType;

typedef struct {
    int canalLength;
    int tickMs;

    int leftInitialShips;
    int rightInitialShips;

    SchedulerType schedulerType;
    FLOWTYPE flowType;

    int fairnessW;
    int signInterval;
    int rrQuantum;

    int demoMaxTicks;
} AppConfig;

void config_load_defaults(AppConfig *config);


#endif //CONFIG_H
