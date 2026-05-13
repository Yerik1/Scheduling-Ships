//
// Created by user on 5/12/2026.
//

#ifndef UI_H
#define UI_H

#include "../queues/ready_queue.h"
#include "../canal/canal.h"
#include "../flow_policies/flow_policy.h"

void ui_bridge_send_state(
    ReadyQueue *leftQueue,
    ReadyQueue *rightQueue,
    Canal *canal,
    FlowPolicy *flowPolicy
);

#endif //UI_H
