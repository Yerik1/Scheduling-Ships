#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "config.h"
#include "ship_factory.h"
#include "ship_task.h"
#include "ready_queue.h"
#include "Scheduler.h"
#include "canal.h"
#include "flow_policy.h"

#define DEMO_TOTAL_LEFT_SHIPS 4
#define DEMO_TOTAL_RIGHT_SHIPS 4
#define DEMO_TOTAL_SHIPS (DEMO_TOTAL_LEFT_SHIPS + DEMO_TOTAL_RIGHT_SHIPS)

static AppConfig config;

static Canal demoCanal;
static ReadyQueue leftQueue;
static ReadyQueue rightQueue;
static FlowPolicy flowPolicy;

static ShipTask leftShips[DEMO_TOTAL_LEFT_SHIPS];
static ShipTask rightShips[DEMO_TOTAL_RIGHT_SHIPS];

static TaskHandle_t simulationTaskHandle = NULL;

static void simulation_task(void *params);
static void demo_ship_task(void *params);

static void create_demo_ships(void);
static void init_demo_ship_task(ShipTask *shipTask, ShipType type, Direction direction, int canalLength);
static void create_ship_freertos_task(ShipTask *shipTask);

static int select_scheduler_index(SchedulerType schedulerType, ReadyQueue *queue);
static const char *flow_decision_to_string(FLOWDECISION decision);
static const char *flow_type_to_string(FLOWTYPE type);
static const char *scheduler_type_to_string(SchedulerType type);
static const char *ship_type_to_string(ShipType type);
static const char *direction_to_string(Direction direction);
static const char *state_to_string(ShipState state);

void app_main(void)
{
    printf("\n\n===============================\n");
    printf("DEMO: Flow Policies + Scheduler + Tasks\n");
    printf("===============================\n\n");

    config_load_defaults(&config);

    /*
     * Cambia estos valores para probar diferentes politicas.
     */
    config.flowType = FLOW_SIGN;
    config.schedulerType = SCHED_STRN;

    config.canalLength = 10;
    config.tickMs = 700;

    config.signInterval = 4;
    config.fairnessW = 2;
    config.rrQuantum = 2;

    config.demoMaxTicks = 50;

    printf("Config:\n");
    printf("- Canal length: %d\n", config.canalLength);
    printf("- Tick ms: %d\n", config.tickMs);
    printf("- Flow policy: %s\n", flow_type_to_string(config.flowType));
    printf("- Scheduler: %s\n", scheduler_type_to_string(config.schedulerType));
    printf("- Sign interval: %d\n", config.signInterval);
    printf("- Fairness W: %d\n", config.fairnessW);
    printf("- Demo max ticks: %d\n\n", config.demoMaxTicks);

    canal_init(&demoCanal, config.canalLength, LEFT);

    queue_init(&leftQueue);
    queue_init(&rightQueue);

    flow_policy_init(
        &flowPolicy,
        &demoCanal,
        FLOW_LEFT,
        config.flowType,
        config.fairnessW,
        config.signInterval
    );

    create_demo_ships();

    BaseType_t result = xTaskCreate(
        simulation_task,
        "SimulationTask",
        4096,
        NULL,
        6,
        &simulationTaskHandle
    );

    if (result != pdPASS) {
        printf("ERROR: No se pudo crear SimulationTask\n");
    }
}

static void create_demo_ships(void)
{
    /*
     * Barcos del lado izquierdo.
     */
    init_demo_ship_task(&leftShips[0], NORMAL, LEFT, config.canalLength);
    init_demo_ship_task(&leftShips[1], FISHING, LEFT, config.canalLength);
    init_demo_ship_task(&leftShips[2], PATROL, LEFT, config.canalLength);
    init_demo_ship_task(&leftShips[3], NORMAL, LEFT, config.canalLength);

    /*
     * Barcos del lado derecho.
     */
    init_demo_ship_task(&rightShips[0], NORMAL, RIGHT, config.canalLength);
    init_demo_ship_task(&rightShips[1], FISHING, RIGHT, config.canalLength);
    init_demo_ship_task(&rightShips[2], PATROL, RIGHT, config.canalLength);
    init_demo_ship_task(&rightShips[3], NORMAL, RIGHT, config.canalLength);

    for (int i = 0; i < DEMO_TOTAL_LEFT_SHIPS; i++) {
        queue_add(&leftQueue, &leftShips[i]);
        create_ship_freertos_task(&leftShips[i]);

        printf(
            "Barco agregado a LEFT queue | ID: %d | Tipo: %s | Velocidad: %d\n",
            leftShips[i].ship.id,
            ship_type_to_string(leftShips[i].ship.type),
            leftShips[i].ship.speed
        );
    }

    for (int i = 0; i < DEMO_TOTAL_RIGHT_SHIPS; i++) {
        queue_add(&rightQueue, &rightShips[i]);
        create_ship_freertos_task(&rightShips[i]);

        printf(
            "Barco agregado a RIGHT queue | ID: %d | Tipo: %s | Velocidad: %d\n",
            rightShips[i].ship.id,
            ship_type_to_string(rightShips[i].ship.type),
            rightShips[i].ship.speed
        );
    }

    printf("\n");
}

static void init_demo_ship_task(
    ShipTask *shipTask,
    ShipType type,
    Direction direction,
    int canalLength
) {
    if (shipTask == NULL) {
        return;
    }

    memset(shipTask, 0, sizeof(ShipTask));

    shipTask->ship = createShip(type, direction, canalLength);
    shipTask->handle = NULL;
    shipTask->maxSteps = canalLength + 5;

    snprintf(
        shipTask->taskName,
        sizeof(shipTask->taskName),
        "Ship_%d",
        shipTask->ship.id
    );
}

static void create_ship_freertos_task(ShipTask *shipTask)
{
    if (shipTask == NULL) {
        return;
    }

    BaseType_t result = xTaskCreate(
        demo_ship_task,
        shipTask->taskName,
        4096,
        shipTask,
        5,
        &shipTask->handle
    );

    if (result != pdPASS) {
        printf("ERROR: No se pudo crear task para barco %d\n", shipTask->ship.id);
    }
}

static void simulation_task(void *params)
{
    (void) params;

    int tick = 0;
    int rrIndexLeft = 0;
    int rrIndexRight = 0;

    printf("SimulationTask iniciada.\n\n");

    while (tick < config.demoMaxTicks) {
        tick++;

        printf("\n---------- TICK %d ----------\n", tick);

        flow_policy_on_tick(&flowPolicy);

        FLOWDECISION decision = flow_policy_select_side(
            &flowPolicy,
            &leftQueue,
            &rightQueue
        );

        printf(
            "FlowPolicy: %s | Direccion actual: %s | Decision: %s\n",
            flow_type_to_string(flowPolicy.type),
            flow_decision_to_string(flowPolicy.direction),
            flow_decision_to_string(decision)
        );

        ReadyQueue *selectedQueue = NULL;
        FLOWDECISION releasedSide = FLOW_NONE;

        if (decision == FLOW_LEFT) {
            selectedQueue = &leftQueue;
            releasedSide = FLOW_LEFT;
        } else if (decision == FLOW_RIGHT) {
            selectedQueue = &rightQueue;
            releasedSide = FLOW_RIGHT;
        }

        if (selectedQueue == NULL || queue_is_empty(selectedQueue)) {
            printf("No se libero ningun barco este tick.\n");
            vTaskDelay(pdMS_TO_TICKS(config.tickMs));
            continue;
        }

        int selectedIndex = -1;

        if (config.schedulerType == SCHED_RR) {
            if (releasedSide == FLOW_LEFT) {
                selectedIndex = scheduler_rr(selectedQueue, config.rrQuantum, &rrIndexLeft);
            } else {
                selectedIndex = scheduler_rr(selectedQueue, config.rrQuantum, &rrIndexRight);
            }
        } else {
            selectedIndex = select_scheduler_index(config.schedulerType, selectedQueue);
        }

        if (selectedIndex < 0) {
            printf("Scheduler no selecciono ningun barco.\n");
            vTaskDelay(pdMS_TO_TICKS(config.tickMs));
            continue;
        }

        ShipTask *selectedTask = queue_get(selectedQueue, selectedIndex);

        if (selectedTask == NULL) {
            printf("La task seleccionada es NULL.\n");
            vTaskDelay(pdMS_TO_TICKS(config.tickMs));
            continue;
        }

        printf(
            "Scheduler selecciono barco %d | Tipo: %s | Origen: %s | Pos: %d\n",
            selectedTask->ship.id,
            ship_type_to_string(selectedTask->ship.type),
            direction_to_string(selectedTask->ship.origin),
            selectedTask->ship.position
        );

        if (canal_enter(&demoCanal, selectedTask)) {
            printf(
                "Barco %d entro exitosamente desde %s\n",
                selectedTask->ship.id,
                flow_decision_to_string(releasedSide)
            );

            queue_remove(selectedQueue, selectedIndex);
            flow_policy_on_ship_released(&flowPolicy, releasedSide);
        } else {
            printf(
                "Barco %d no pudo entrar al canal todavia.\n",
                selectedTask->ship.id
            );
        }

        if (queue_is_empty(&leftQueue) &&
            queue_is_empty(&rightQueue) &&
            canal_is_empty(&demoCanal)) {
            printf("\nNo quedan barcos en colas ni en canal.\n");
            break;
        }

        vTaskDelay(pdMS_TO_TICKS(config.tickMs));
    }

    printf("\nSimulationTask finalizada.\n");
    vTaskDelete(NULL);
}

static void demo_ship_task(void *params)
{
    ShipTask *shipTask = (ShipTask *) params;

    if (shipTask == NULL) {
        vTaskDelete(NULL);
        return;
    }

    printf(
        "[%s] Task iniciada | ID: %d | Tipo: %s | Origen: %s | Estado: %s\n",
        shipTask->taskName,
        shipTask->ship.id,
        ship_type_to_string(shipTask->ship.type),
        direction_to_string(shipTask->ship.origin),
        state_to_string(shipTask->ship.state)
    );

    /*
     * La task espera hasta que SimulationTask la meta al canal.
     * canal_enter() cambia el estado a RUNNING.
     */
    while (shipTask->ship.state != RUNNING) {
        vTaskDelay(pdMS_TO_TICKS(200));
    }

    printf(
        "[%s] Detecto entrada al canal | Posicion: %d\n",
        shipTask->taskName,
        shipTask->ship.position
    );

    while (shipTask->ship.state != FINISHED) {
        bool moved = canal_move_task(&demoCanal, shipTask);

        if (!moved) {
            printf(
                "[%s] No pudo moverse | Posicion actual: %d\n",
                shipTask->taskName,
                shipTask->ship.position
            );
        }

        vTaskDelay(pdMS_TO_TICKS(500));
    }

    printf(
        "[%s] Finalizo cruce | Estado: %s\n",
        shipTask->taskName,
        state_to_string(shipTask->ship.state)
    );

    vTaskDelete(NULL);
}

static int select_scheduler_index(SchedulerType schedulerType, ReadyQueue *queue)
{
    switch (schedulerType) {
        case SCHED_FCFS:
            return scheduler_fcfs(queue);

        case SCHED_SJF:
            return scheduler_sjf(queue);

        case SCHED_STRN:
            return scheduler_strn(queue);

        case SCHED_PRIORITY:
            return scheduler_priority(queue);

        case SCHED_EDF:
            return scheduler_edf(queue);

        case SCHD_RR:
            /*
             * RR se maneja aparte porque necesita rrIndex.
             */
            return scheduler_fcfs(queue);

        default:
            return -1;
    }
}

static const char *flow_decision_to_string(FLOWDECISION decision)
{
    switch (decision) {
        case FLOW_LEFT:
            return "FLOW_LEFT";
        case FLOW_RIGHT:
            return "FLOW_RIGHT";
        case FLOW_NONE:
            return "FLOW_NONE";
        default:
            return "UNKNOWN";
    }
}

static const char *flow_type_to_string(FLOWTYPE type)
{
    switch (type) {
        case FLOW_FAIRNESS:
            return "FLOW_FAIRNESS";
        case FLOW_SIGN:
            return "FLOW_SIGN";
        case FLOW_TICO:
            return "FLOW_TICO";
        default:
            return "UNKNOWN";
    }
}

static const char *scheduler_type_to_string(SchedulerType type)
{
    switch (type) {
        case SCHED_FCFS:
            return "SCHED_FCFS";
        case SCHD_RR:
            return "SCHED_RR";
        case SCHED_PRIORITY:
            return "SCHED_PRIORITY";
        case SCHED_SJF:
            return "SCHED_SJF";
        case SCHED_STRN:
            return "SCHED_STRN";
        case SCHED_EDF:
            return "SCHED_EDF";
        default:
            return "UNKNOWN";
    }
}

static const char *ship_type_to_string(ShipType type)
{
    switch (type) {
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

static const char *direction_to_string(Direction direction)
{
    switch (direction) {
        case LEFT:
            return "LEFT";
        case RIGHT:
            return "RIGHT";
        default:
            return "UNKNOWN";
    }
}

static const char *state_to_string(ShipState state)
{
    switch (state) {
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