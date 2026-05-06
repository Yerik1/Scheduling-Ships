//
// Created by user on 4/27/2026.
//

#ifndef FLOW_POLICY_H
#define FLOW_POLICY_H

#include "../canal/canal.h"
#include "../queues/ready_queue.h"

typedef enum {FLOW_FAIRNESS, FLOW_SIGN, FLOW_TICO} FLOWTYPE;
typedef enum {FLOW_LEFT, FLOW_RIGHT, FLOW_NONE} FLOWDECISION;
typedef struct {
    Canal *canal;
    FLOWDECISION direction;
    FLOWTYPE type;
    int w; //cantidad barcos por ciclo
    int wCount;
    int signInterval; //tiempo letrero
    int elapsedTicks;

}FlowPolicy;

void flow_policy_init(FlowPolicy *flow_policy, Canal *canal, FLOWDECISION direction, FLOWTYPE type, int w, int signInterval);
void flow_policy_on_tick(FlowPolicy *flow_policy);
FLOWDECISION flow_policy_select_side(FlowPolicy *flow_policy, ReadyQueue *leftQ, ReadyQueue *rightQ);
void flow_policy_on_ship_released(FlowPolicy *flow_policy, FLOWDECISION releasedDirection);
static bool has_ships(ReadyQueue *queue);
static FLOWDECISION opposite_direction(FLOWDECISION direction);
static FLOWDECISION direction_to_flow_decision(Direction direction);
static bool canal_allows_side(FlowPolicy *flow_policy, FLOWDECISION side);


#endif //FLOW_POLICY_H
