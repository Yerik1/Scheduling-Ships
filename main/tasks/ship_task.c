//
// Created by user on 4/27/2026.
//

#include "ship_task.h"

#include <stdio.h>
#include <string.h>

static void shipTaskEntry(void *params);

static const char *shipTypeToString(ShipType type)
{
    switch (type)
    {
    case NORMAL:
        return "NORMAL";
    case FISHING:
        return "FISHING";
    case PATROL:
        return "PATROL";
    default:
        return "UNKNOWN";
    }
}

static const char *directionToString(Direction direction)
{
    switch (direction)
    {
    case LEFT:
        return "LEFT";
    case RIGHT:
        return "RIGHT";
    default:
        return "UNKNOWN";
    }
}

static const char *shipStateToString(ShipState state)
{
    switch (state)
    {
    case READY:
        return "READY";
    case WAITING:
        return "WAITING";
    case RUNNING:
        return "RUNNING";
    case BLOCKED:
        return "BLOCKED";
    case FINISHED:
        return "FINISHED";
    default:
        return "UNKNOWN";
    }
}

BaseType_t createShipTask(struct Ship ship)
{
    return createShipTaskWithConfig(
        ship,
        SHIP_TASK_DEFAULT_RTOS_PRIORITY,
        SHIP_TASK_DEFAULT_STACK_SIZE,
        SHIP_TASK_DEFAULT_STEPS);
}

BaseType_t createShipTaskWithConfig(
    struct Ship ship,
    UBaseType_t rtosPriority,
    uint32_t stackSizeBytes,
    int maxSteps)
{
    if (stackSizeBytes == 0)
    {
        stackSizeBytes = SHIP_TASK_DEFAULT_STACK_SIZE;
    }

    if (maxSteps <= 0)
    {
        maxSteps = SHIP_TASK_DEFAULT_STEPS;
    }

    ShipTask *shipTask = pvPortMalloc(sizeof(ShipTask));

    if (shipTask == NULL)
    {
        printf("ERROR: No se pudo reservar memoria para ShipTask\n");
        return pdFAIL;
    }

    shipTask->ship = ship;
    shipTask->handle = NULL;
    shipTask->maxSteps = maxSteps;
    shipTask->hasEnteredBefore = false;
    shipTask->justEntered = false;

    snprintf(
        shipTask->taskName,
        sizeof(shipTask->taskName),
        "Ship_%d",
        ship.id);

    BaseType_t result = xTaskCreate(
        shipTaskEntry,
        shipTask->taskName,
        stackSizeBytes,
        shipTask,
        rtosPriority,
        &shipTask->handle);

    if (result != pdPASS)
    {
        printf("ERROR: No se pudo crear la task del barco %d\n", ship.id);
        vPortFree(shipTask);
        return pdFAIL;
    }

    printf("Task creada para barco %d con nombre %s\n", ship.id, shipTask->taskName);

    return pdPASS;
}

static void shipTaskEntry(void *params)
{
    ShipTask *shipTask = (ShipTask *)params;

    if (shipTask == NULL)
    {
        vTaskDelete(NULL);
        return;
    }

    struct Ship *ship = &shipTask->ship;

    printf(
        "[%s] Barco creado | ID: %d | Tipo: %s | Origen: %s | Destino: %s | Estado: %s\n",
        shipTask->taskName,
        ship->id,
        shipTypeToString(ship->type),
        directionToString(ship->origin),
        directionToString(ship->destination),
        shipStateToString(ship->state));

    ship_set_state(ship, RUNNING);

    printf(
        "[%s] Barco %d cambio a estado %s\n",
        shipTask->taskName,
        ship->id,
        shipStateToString(ship->state));

    int stepsToRun = ship->remaining_time;

    if (stepsToRun <= 0)
    {
        stepsToRun = shipTask->maxSteps;
    }

    for (int i = 0; i < stepsToRun; i++)
    {
        ship_update_position(ship);
        ship_decrement_remaining_time(ship);

        printf(
            "[%s] Barco %d avanzando | Posicion: %d | Tiempo restante: %d | Velocidad: %d\n",
            shipTask->taskName,
            ship->id,
            ship->position,
            ship->remaining_time,
            ship_get_speed(ship));

        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    ship_finish(ship);

    printf(
        "[%s] Barco %d finalizo | Estado: %s\n",
        shipTask->taskName,
        ship->id,
        shipStateToString(ship->state));

    vPortFree(shipTask);
    vTaskDelete(NULL);
}
