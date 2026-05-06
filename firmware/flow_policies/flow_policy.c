//
// Created by user on 5/3/2026.
//

#include "flow_policy.h"

void flow_policy_init(FlowPolicy *flow_policy, Canal *canal, FLOWDECISION direction, FLOWTYPE type, int w, int signInterval) {
    if (flow_policy == NULL) {
        return;
    }
    flow_policy->canal = canal;
    flow_policy->direction = direction;
    flow_policy->type = type;

    flow_policy->w = w;
    flow_policy->wCount = 0;

    flow_policy->signInterval = signInterval;
    flow_policy->elapsedTicks = 0;

    if (flow_policy->w <= 0) {
        flow_policy->w = 1;
    }

    if (flow_policy->signInterval <= 0) {
        flow_policy->signInterval = 1;
    }

    if (flow_policy->direction == FLOW_NONE) {
        flow_policy->direction = FLOW_LEFT;
    }

}
void flow_policy_on_tick(FlowPolicy *flow_policy) {
    if (flow_policy == NULL) {
        return;
    }
    if (flow_policy->type != FLOW_SIGN) {
        return;
    }
    flow_policy->elapsedTicks++;
    if (flow_policy->elapsedTicks >= flow_policy->signInterval) {
        if (flow_policy->direction == FLOW_LEFT) {
            flow_policy->direction = FLOW_RIGHT;
        }
        else if (flow_policy->direction == FLOW_RIGHT) {
            flow_policy->direction = FLOW_LEFT;
        }
        flow_policy->elapsedTicks = 0;
    }
}
FLOWDECISION flow_policy_select_side(FlowPolicy *flow_policy, ReadyQueue *leftQ, ReadyQueue *rightQ) {
    if (flow_policy == NULL) {
        return FLOW_NONE;
    }
    bool leftHasShips = has_ships(leftQ);
    bool rightHasShips = has_ships(rightQ);

    if (!leftHasShips && !rightHasShips) {
        return FLOW_NONE;
    }

    switch (flow_policy->type) {
        case FLOW_SIGN:
            /*
             * Letrero:
             * Si el letrero apunta a LEFT, solo intenta LEFT.
             * Si apunta a RIGHT, solo intenta RIGHT.
             */
            if (flow_policy->direction == FLOW_LEFT &&
                leftHasShips &&
                canal_allows_side(flow_policy, FLOW_LEFT)) {
                return FLOW_LEFT;
                }

            if (flow_policy->direction == FLOW_RIGHT &&
                rightHasShips &&
                canal_allows_side(flow_policy, FLOW_RIGHT)) {
                return FLOW_RIGHT;
                }

            return FLOW_NONE;

        case FLOW_FAIRNESS:
            /*
             * Equidad:
             * Intenta liberar W barcos del lado actual.
             * Si el lado actual no tiene barcos, deja pasar del otro lado
             * para garantizar flujo.
             */
            if (flow_policy->direction == FLOW_LEFT) {
                if (leftHasShips && canal_allows_side(flow_policy, FLOW_LEFT)) {
                    return FLOW_LEFT;
                }

                if (!leftHasShips &&
                    rightHasShips &&
                    canal_allows_side(flow_policy, FLOW_RIGHT)) {
                    return FLOW_RIGHT;
                    }

                return FLOW_NONE;
            }

            if (flow_policy->direction == FLOW_RIGHT) {
                if (rightHasShips && canal_allows_side(flow_policy, FLOW_RIGHT)) {
                    return FLOW_RIGHT;
                }

                if (!rightHasShips &&
                    leftHasShips &&
                    canal_allows_side(flow_policy, FLOW_LEFT)) {
                    return FLOW_LEFT;
                    }

                return FLOW_NONE;
            }

            return FLOW_NONE;

        case FLOW_TICO:
            /*
             * Tico:
             * No hay control fijo de flujo.
             * Se intenta alternar para no favorecer siempre el mismo lado.
             * El canal sigue siendo quien valida que no haya colisiones.
             */
            if (flow_policy->direction == FLOW_LEFT) {
                if (rightHasShips && canal_allows_side(flow_policy, FLOW_RIGHT)) {
                    return FLOW_RIGHT;
                }

                if (leftHasShips && canal_allows_side(flow_policy, FLOW_LEFT)) {
                    return FLOW_LEFT;
                }

                return FLOW_NONE;
            }

            if (flow_policy->direction == FLOW_RIGHT) {
                if (leftHasShips && canal_allows_side(flow_policy, FLOW_LEFT)) {
                    return FLOW_LEFT;
                }

                if (rightHasShips && canal_allows_side(flow_policy, FLOW_RIGHT)) {
                    return FLOW_RIGHT;
                }

                return FLOW_NONE;
            }

            if (leftHasShips && canal_allows_side(flow_policy, FLOW_LEFT)) {
                return FLOW_LEFT;
            }

            if (rightHasShips && canal_allows_side(flow_policy, FLOW_RIGHT)) {
                return FLOW_RIGHT;
            }

            return FLOW_NONE;

        default:
            return FLOW_NONE;
    }
}
void flow_policy_on_ship_released(FlowPolicy *flow_policy, FLOWDECISION releasedDirection) {
    if (flow_policy == NULL) {
        return;
    }
    if (releasedDirection == FLOW_NONE) {
        return;
    }
    if (flow_policy->type == FLOW_FAIRNESS) {
        /*
         * Si por garantía de flujo se liberó un barco del lado contrario,
         * sincronizamos la dirección actual con ese lado.
         */
        if (flow_policy->direction != releasedDirection) {
            flow_policy->direction = releasedDirection;
            flow_policy->wCount = 0;
        }

        flow_policy->wCount++;

        if (flow_policy->wCount >= flow_policy->w) {
            flow_policy->direction = opposite_direction(flow_policy->direction);
            flow_policy->wCount = 0;
        }

        return;
    }

    if (flow_policy->type == FLOW_TICO) {
        /*
         * Tico usa direction como "último lado liberado",
         * para intentar alternar en la próxima selección.
         */
        flow_policy->direction = releasedDirection;
        return;
    }

    /*
     * Para FLOW_SIGN no se cambia direction aquí,
     * porque el letrero cambia solo por ticks.
     */
}

static bool has_ships(ReadyQueue *queue) {
    if (queue == NULL) {
        return false;
    }

    return !queue_is_empty(queue);
}

static FLOWDECISION opposite_direction(FLOWDECISION direction) {
    if (direction == FLOW_LEFT) {
        return FLOW_RIGHT;
    }

    if (direction == FLOW_RIGHT) {
        return FLOW_LEFT;
    }

    return FLOW_NONE;
}

static FLOWDECISION direction_to_flow_decision(Direction direction) {
    if (direction == LEFT) {
        return FLOW_LEFT;
    }

    return FLOW_RIGHT;
}

static bool canal_allows_side(FlowPolicy *flow_policy, FLOWDECISION side) {
    if (flow_policy == NULL) {
        return false;
    }

    if (side == FLOW_NONE) {
        return false;
    }

    Canal *canal = flow_policy->canal;

    if (canal == NULL) {
        return true;
    }

    /*
     * Si el canal está bloqueado por interrupción/agujas,
     * no se permite liberar barcos.
     */
    if (canal->isBlocked) {
        return false;
    }

    /*
     * Si el canal está vacío, cualquiera de los dos lados podría intentar entrar.
     * canal_enter() seguirá siendo la autoridad final.
     */
    if (canal_is_empty(canal)) {
        return true;
    }

    /*
     * Si ya hay barcos dentro, solo se permite intentar liberar más barcos
     * del mismo sentido actual del canal.
     */
    FLOWDECISION canalDirection = direction_to_flow_decision(canal->current_direction);

    return canalDirection == side;
}