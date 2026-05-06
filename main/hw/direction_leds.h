//
// Created by user on 5/5/2026.
//

#ifndef DIRECTION_LEDS_H
#define DIRECTION_LEDS_H

#include <stdbool.h>

#include "../flow_policies/flow_policy.h"

bool direction_leds_init(void);

void direction_leds_set(FLOWDECISION direction);

void direction_leds_clear(void);

#endif //DIRECTION_LEDS_H
