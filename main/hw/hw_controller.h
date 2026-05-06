//
// Created by user on 5/5/2026.
//

#ifndef HW_CONTROLLER_H
#define HW_CONTROLLER_H

#include <stdbool.h>

#include "../queues/ready_queue.h"
#include "../canal/canal.h"
#include "../flow_policies/flow_policy.h"
#include "led_strips.h"
#include "direction_leds.h"
#include "proximity_sensor.h"

bool hardware_init(void);

void hardware_render_state(
    ReadyQueue *leftQueue,
    ReadyQueue *rightQueue,
    Canal *canal,
    FlowPolicy *flowPolicy
);

bool hardware_interrupt_triggered(void);

void hardware_clear_interrupt(void);

void hardware_set_enabled(bool enabled);

#endif //HW_CONTROLLER_H
