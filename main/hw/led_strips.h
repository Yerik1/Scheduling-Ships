//
// Created by user on 5/5/2026.
//

#ifndef LED_STRIPS_H
#define LED_STRIPS_H

#include <stdbool.h>

#include "../queues/ready_queue.h"
#include "../canal/canal.h"

bool led_strips_init(void);

void led_strips_render_left_queue(ReadyQueue *leftQueue, int hwVisibleQueueSlots);
void led_strips_render_right_queue(ReadyQueue *rightQueue, int hwVisibleQueueSlots);
void led_strips_render_canal(Canal *canal);
bool led_strips_init_canal_only(void);

void led_strips_clear(void);

#endif //LED_STRIPS_H
