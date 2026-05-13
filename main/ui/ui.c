//
// Created by user on 5/12/2026.
//

#include "ui.h"

#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>

#include "serial_comm.h"

#define UI_STATE_BUFFER_SIZE 1024

static char ship_type_to_char(ShipType type);
static const char *flow_type_to_string(FLOWTYPE type);
static char flow_direction_to_sign(FlowPolicy *flowPolicy, Canal *canal);
static int append_text(char *buffer, size_t bufferSize, int offset, const char *format, ...);

static void append_queue_state(
    char *buffer,
    size_t bufferSize,
    int *offset,
    ReadyQueue *queue
);

static void append_canal_state(
    char *buffer,
    size_t bufferSize,
    int *offset,
    Canal *canal
);

void ui_bridge_send_state(
    ReadyQueue *leftQueue,
    ReadyQueue *rightQueue,
    Canal *canal,
    FlowPolicy *flowPolicy
) {
    if (leftQueue == NULL || rightQueue == NULL || canal == NULL || flowPolicy == NULL) {
        return;
    }

    char buffer[UI_STATE_BUFFER_SIZE];
    int offset = 0;

    buffer[0] = '\0';

    offset = append_text(buffer, sizeof(buffer), offset, "STATE:left:");

    append_queue_state(buffer, sizeof(buffer), &offset, leftQueue);

    offset = append_text(buffer, sizeof(buffer), offset, ";right:");

    append_queue_state(buffer, sizeof(buffer), &offset, rightQueue);

    offset = append_text(buffer, sizeof(buffer), offset, ";canal:");

    append_canal_state(buffer, sizeof(buffer), &offset, canal);

    char sign = flow_direction_to_sign(flowPolicy, canal);
    int needle = (canal->isBlocked || canal->leftGateDown || canal->rightGateDown) ? 1 : 0;

    offset = append_text(
        buffer,
        sizeof(buffer),
        offset,
        ";sign:%c;needle:%d;flow:%s\n",
        sign,
        needle,
        flow_type_to_string(flowPolicy->type)
    );

    serial_comm_write_text(buffer);
}

static void append_queue_state(
    char *buffer,
    size_t bufferSize,
    int *offset,
    ReadyQueue *queue
) {
    if (buffer == NULL || offset == NULL || queue == NULL) {
        return;
    }

    for (int i = 0; i < queue->count; i++) {
        ShipTask *task = queue->tasks[i];

        if (task == NULL) {
            continue;
        }

        char side = task->ship.origin == LEFT ? 'L' : 'R';
        char type = ship_type_to_char(task->ship.type);

        if (i > 0) {
            *offset = append_text(buffer, bufferSize, *offset, ",");
        }

        *offset = append_text(
            buffer,
            bufferSize,
            *offset,
            "%c%d%c",
            side,
            task->ship.id,
            type
        );
    }
}

static void append_canal_state(
    char *buffer,
    size_t bufferSize,
    int *offset,
    Canal *canal
) {
    if (buffer == NULL || offset == NULL || canal == NULL) {
        return;
    }

    bool first = true;

    for (int pos = 0; pos < canal->length; pos++) {
        ShipTask *task = canal->ships_inside.tasks[pos];

        if (task == NULL) {
            continue;
        }

        char side = task->ship.origin == LEFT ? 'L' : 'R';
        char type = ship_type_to_char(task->ship.type);

        if (!first) {
            *offset = append_text(buffer, bufferSize, *offset, ",");
        }

        *offset = append_text(
            buffer,
            bufferSize,
            *offset,
            "%c%d%c@%d#%c",
            side,
            task->ship.id,
            type,
            task->ship.position,
            type
        );

        first = false;
    }
}

static char ship_type_to_char(ShipType type) {
    switch (type) {
        case NORMAL:
            return 'N';

        case FISHING:
            return 'F';

        case PATROL:
            return 'P';

        default:
            return 'N';
    }
}

static const char *flow_type_to_string(FLOWTYPE type) {
    switch (type) {
        case FLOW_FAIRNESS:
            return "Equidad";

        case FLOW_SIGN:
            return "Letrero";

        case FLOW_TICO:
            return "Tico";

        default:
            return "Equidad";
    }
}

static char flow_direction_to_sign(FlowPolicy *flowPolicy, Canal *canal) {
    if (flowPolicy != NULL) {
        if (flowPolicy->direction == FLOW_LEFT) {
            return 'L';
        }

        if (flowPolicy->direction == FLOW_RIGHT) {
            return 'R';
        }
    }

    if (canal != NULL) {
        if (canal->current_direction == LEFT) {
            return 'L';
        }

        if (canal->current_direction == RIGHT) {
            return 'R';
        }
    }

    return 'L';
}

static int append_text(char *buffer, size_t bufferSize, int offset, const char *format, ...) {
    if (buffer == NULL || format == NULL || offset < 0 || offset >= (int) bufferSize) {
        return offset;
    }

    va_list args;
    va_start(args, format);

    int written = vsnprintf(
        buffer + offset,
        bufferSize - offset,
        format,
        args
    );

    va_end(args);

    if (written < 0) {
        return offset;
    }

    offset += written;

    if (offset >= (int) bufferSize) {
        offset = (int) bufferSize - 1;
        buffer[offset] = '\0';
    }

    return offset;
}