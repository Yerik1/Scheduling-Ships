#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "config.h"
#include "ship_factory.h"
#include "ship_task.h"
#include "ready_queue.h"
#include "Scheduler.h"
#include "canal.h"
#include "flow_policy.h"
#include "hw_controller.h"
#include "ui.h"
#include "serial_comm.h"

#define UI_LINE_BUFFER_SIZE 160
#define SHIP_TASK_STACK_SIZE 4096
#define SHIP_TASK_PRIORITY 5
#define SIMULATION_TASK_STACK_SIZE 4096
#define SIMULATION_TASK_PRIORITY 6
#define COMMAND_TASK_STACK_SIZE 4096
#define COMMAND_TASK_PRIORITY 4

static AppConfig config;

static Canal demoCanal;
static ReadyQueue leftQueue;
static ReadyQueue rightQueue;
static FlowPolicy flowPolicy;

static TaskHandle_t simulationTaskHandle = NULL;
static TaskHandle_t commandTaskHandle = NULL;

static SemaphoreHandle_t queuesSemaphore = NULL;

static volatile bool systemRunning = false;
static bool dynamicGenerationEnabled = false;
static int queueCapacity = 4;

static void simulation_task(void *params);
static void command_task(void *params);
static void ship_task_entry(void *params);

static void wait_for_ui_start(void);
static bool process_conf_command(char *line);
static bool process_gen_command(char *line, bool isInitialLoad);
static bool create_ship_from_ui(char sideChar, char typeChar);

static void render_outputs(void);
static void trim_newline(char *line);

static SchedulerType scheduler_from_string(const char *text);
static FLOWTYPE flow_from_string(const char *text);
static ShipType ship_type_from_char(char typeChar);
static Direction direction_from_char(char sideChar);

static int select_scheduler_index(SchedulerType schedulerType, ReadyQueue *queue);

static const char *flow_decision_to_string(FLOWDECISION decision);
static const char *flow_type_to_string(FLOWTYPE type);
static const char *scheduler_type_to_string(SchedulerType type);
static const char *ship_type_to_string(ShipType type);
static const char *direction_to_string(Direction direction);
static const char *state_to_string(ShipState state);

static bool take_queues(void);
static void give_queues(void);
static bool configReceived = false;

void app_main(void)
{
    printf("\n\n===============================\n");
    printf("Scheduling Ships - UI Mode\n");
    printf("===============================\n\n");

    config_load_defaults(&config);

    queue_init(&leftQueue);
    queue_init(&rightQueue);

    queuesSemaphore = xSemaphoreCreateBinary();

    if (queuesSemaphore == NULL) {
        printf("ERROR: No se pudo crear queuesSemaphore\n");
        return;
    }

    xSemaphoreGive(queuesSemaphore);

    bool hardwareOk = hardware_init();

    if (!hardwareOk) {
        printf("ADVERTENCIA: Hardware no inicializado correctamente. La simulacion continua.\n");
        hardware_set_enabled(false);
    }

    if (!serial_comm_init()) {
        printf("ERROR: No se pudo inicializar serial_comm\n");
        return;
    }

    wait_for_ui_start();

    systemRunning = true;

    render_outputs();

    BaseType_t simResult = xTaskCreate(
        simulation_task,
        "SimulationTask",
        SIMULATION_TASK_STACK_SIZE,
        NULL,
        SIMULATION_TASK_PRIORITY,
        &simulationTaskHandle
    );

    if (simResult != pdPASS) {
        printf("ERROR: No se pudo crear SimulationTask\n");
        systemRunning = false;
        return;
    }

    BaseType_t cmdResult = xTaskCreate(
        command_task,
        "CommandTask",
        COMMAND_TASK_STACK_SIZE,
        NULL,
        COMMAND_TASK_PRIORITY,
        &commandTaskHandle
    );

    if (cmdResult != pdPASS) {
        printf("ADVERTENCIA: No se pudo crear CommandTask. No habra generacion dinamica.\n");
    }
}

static bool process_cfg_command(char *line)
{
    if (strncmp(line, "CFG_LEN:", 8) == 0) {
        config.canalLength = atoi(line + 8);
        printf("CFG canalLength=%d\n", config.canalLength);
        return true;
    }

    if (strncmp(line, "CFG_Q:", 6) == 0) {
        queueCapacity = atoi(line + 6);

        if (queueCapacity <= 0) {
            queueCapacity = 1;
        }

        if (queueCapacity > 4) {
            queueCapacity = 4;
        }

        printf("CFG queueCapacity=%d\n", queueCapacity);
        return true;
    }

    if (strncmp(line, "CFG_S:", 6) == 0) {
        config.schedulerType = scheduler_from_string(line + 6);
        printf("CFG scheduler=%s\n", line + 6);
        return true;
    }

    if (strncmp(line, "CFG_F:", 6) == 0) {
        config.flowType = flow_from_string(line + 6);
        printf("CFG flow=%s\n", line + 6);
        return true;
    }

    if (strncmp(line, "CFG_W:", 6) == 0) {
        config.fairnessW = atoi(line + 6);

        if (config.fairnessW <= 0) {
            config.fairnessW = 1;
        }

        printf("CFG fairnessW=%d\n", config.fairnessW);
        return true;
    }

    if (strncmp(line, "CFG_SIGN:", 9) == 0) {
        int signIntervalMs = atoi(line + 9);

        if (config.tickMs <= 0) {
            config.tickMs = 500;
        }

        config.signInterval = (signIntervalMs + config.tickMs - 1) / config.tickMs;

        if (config.signInterval <= 0) {
            config.signInterval = 1;
        }

        printf("CFG signInterval ticks=%d\n", config.signInterval);
        return true;
    }

    if (strncmp(line, "CFG_RR:", 7) == 0) {
        config.rrQuantum = atoi(line + 7);

        if (config.rrQuantum <= 0) {
            config.rrQuantum = 1;
        }

        printf("CFG rrQuantum=%d\n", config.rrQuantum);
        return true;
    }

    if (strncmp(line, "CFG_MODE:", 9) == 0) {
        char *mode = line + 9;

        dynamicGenerationEnabled = false;

        if (strncmp(mode, "Din", 3) == 0) {
            dynamicGenerationEnabled = true;
        }

        printf("CFG dynamic=%s\n", dynamicGenerationEnabled ? "SI" : "NO");
        return true;
    }

    return false;
}

static void wait_for_ui_start(void)
{
    char line[UI_LINE_BUFFER_SIZE];
    bool started = false;

    printf("Esperando configuracion desde la interfaz...\n");

    while (!started) {
        int len = serial_comm_read_line(line, sizeof(line));

        if (len <= 0) {
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }

        printf("RX UI: %s\n", line);

        if (strcmp(line, "PING") == 0) {
            serial_comm_write_line("ACK:PING");
            continue;
        }

        if (strncmp(line, "CFG_", 4) == 0) {
            process_cfg_command(line);
            serial_comm_write_line("ACK:CFG");
            continue;
        }

        if (strncmp(line, "GEN:", 4) == 0) {
            char copy[UI_LINE_BUFFER_SIZE];
            strncpy(copy, line, sizeof(copy));
            copy[sizeof(copy) - 1] = '\0';

            process_gen_command(copy, true);
            serial_comm_write_line("ACK:GEN");
            continue;
        }

        if (strcmp(line, "START") == 0) {
            canal_init(&demoCanal, config.canalLength, LEFT);

            flow_policy_init(
                &flowPolicy,
                &demoCanal,
                FLOW_LEFT,
                config.flowType,
                config.fairnessW,
                config.signInterval
            );

            serial_comm_write_line("ACK:START");
            started = true;
            continue;
        }

        printf("Comando desconocido antes de START: %s\n", line);
        serial_comm_write_line("ERR:UNKNOWN");
    }

    printf("START recibido. Iniciando simulacion.\n");
}

static bool process_gen_command(char *line, bool isInitialLoad)
{
    char *token = strtok(line, ":");

    if (token == NULL || strcmp(token, "GEN") != 0) {
        return false;
    }

    char *sideStr = strtok(NULL, ":");
    char *typeStr = strtok(NULL, ":");

    if (sideStr == NULL || typeStr == NULL) {
        printf("ERROR: GEN incompleto\n");
        return false;
    }

    if (!isInitialLoad && !dynamicGenerationEnabled) {
        printf("Modo fijo activo. GEN ignorado: %s:%s\n", sideStr, typeStr);
        return false;
    }

    return create_ship_from_ui(sideStr[0], typeStr[0]);
}

static bool create_ship_from_ui(char sideChar, char typeChar)
{
    Direction direction = direction_from_char(sideChar);
    ShipType type = ship_type_from_char(typeChar);

    ShipTask *shipTask = pvPortMalloc(sizeof(ShipTask));

    if (shipTask == NULL) {
        printf("ERROR: No se pudo reservar memoria para ShipTask\n");
        return false;
    }

    memset(shipTask, 0, sizeof(ShipTask));

    shipTask->ship = createShip(type, direction, config.canalLength);
    shipTask->handle = NULL;
    shipTask->maxSteps = config.canalLength + 5;

    snprintf(
        shipTask->taskName,
        sizeof(shipTask->taskName),
        "Ship_%d",
        shipTask->ship.id
    );

    BaseType_t result = xTaskCreate(
        ship_task_entry,
        shipTask->taskName,
        SHIP_TASK_STACK_SIZE,
        shipTask,
        SHIP_TASK_PRIORITY,
        &shipTask->handle
    );

    if (result != pdPASS) {
        printf("ERROR: No se pudo crear task para barco %d\n", shipTask->ship.id);
        vPortFree(shipTask);
        return false;
    }

    ReadyQueue *targetQueue = direction == LEFT ? &leftQueue : &rightQueue;

    if (!take_queues()) {
        printf("ERROR: No se pudo tomar semaforo de colas\n");
        vTaskDelete(shipTask->handle);
        vPortFree(shipTask);
        return false;
    }

    if (targetQueue->count >= queueCapacity) {
        printf(
            "ERROR: Cola %s llena. No se pudo agregar barco %d\n",
            direction_to_string(direction),
            shipTask->ship.id
        );

        give_queues();

        vTaskDelete(shipTask->handle);
        vPortFree(shipTask);

        return false;
    }

    if (!queue_add(targetQueue, shipTask)) {
        printf("ERROR: queue_add fallo para barco %d\n", shipTask->ship.id);

        give_queues();

        vTaskDelete(shipTask->handle);
        vPortFree(shipTask);

        return false;
    }

    give_queues();

    printf(
        "Barco creado desde UI | ID: %d | Lado: %c | Tipo: %c | Velocidad: %d\n",
        shipTask->ship.id,
        sideChar,
        typeChar,
        shipTask->ship.speed
    );

    return true;
}

static void simulation_task(void *params)
{
    (void) params;

    int tick = 0;
    int rrIndexLeft = 0;
    int rrIndexRight = 0;

    printf("SimulationTask iniciada.\n\n");

    while (systemRunning) {
        tick++;

        printf("\n---------- TICK %d ----------\n", tick);

        flow_policy_on_tick(&flowPolicy);

        if (!take_queues()) {
            printf("No se pudo tomar semaforo de colas en SimulationTask.\n");
            render_outputs();
            vTaskDelay(pdMS_TO_TICKS(config.tickMs));
            continue;
        }

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

            bool fixedFinished =
                !dynamicGenerationEnabled &&
                queue_is_empty(&leftQueue) &&
                queue_is_empty(&rightQueue) &&
                canal_is_empty(&demoCanal);

            give_queues();

            render_outputs();

            if (fixedFinished) {
                printf("\nNo quedan barcos en colas ni en canal. Simulacion fija finalizada.\n");
                break;
            }

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
            give_queues();
            render_outputs();
            vTaskDelay(pdMS_TO_TICKS(config.tickMs));
            continue;
        }

        ShipTask *selectedTask = queue_get(selectedQueue, selectedIndex);

        if (selectedTask == NULL) {
            printf("La task seleccionada es NULL.\n");
            give_queues();
            render_outputs();
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

        bool fixedFinished =
            !dynamicGenerationEnabled &&
            queue_is_empty(&leftQueue) &&
            queue_is_empty(&rightQueue) &&
            canal_is_empty(&demoCanal);

        give_queues();

        render_outputs();

        if (fixedFinished) {
            printf("\nNo quedan barcos en colas ni en canal. Simulacion fija finalizada.\n");
            break;
        }

        vTaskDelay(pdMS_TO_TICKS(config.tickMs));
    }

    systemRunning = false;

    render_outputs();

    printf("\nSimulationTask finalizada.\n");

    vTaskDelete(NULL);
}

static void command_task(void *params)
{
    (void) params;

    char line[UI_LINE_BUFFER_SIZE];

    printf("CommandTask iniciada. Esperando comandos dinamicos.\n");

    while (systemRunning) {
        int len = serial_comm_read_line(line, sizeof(line));

        if (len <= 0) {
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }

        trim_newline(line);

        if (strlen(line) == 0) {
            continue;
        }

        printf("RX UI runtime: %s\n", line);

        if (strncmp(line, "GEN:", 4) == 0) {
            char copy[UI_LINE_BUFFER_SIZE];
            strncpy(copy, line, sizeof(copy));
            copy[sizeof(copy) - 1] = '\0';

            if (process_gen_command(copy, false)) {
                serial_comm_write_line("ACK:GEN");
            } else {
                serial_comm_write_line("ERR:GEN");
            }

            render_outputs();
        } else if (strcmp(line, "STOP") == 0) {
            printf("STOP recibido desde UI.\n");
            systemRunning = false;
            break;
        } else {
            printf("Comando runtime ignorado: %s\n", line);
        }
    }

    printf("CommandTask finalizada.\n");
    vTaskDelete(NULL);
}

static void ship_task_entry(void *params)
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

    while (systemRunning && shipTask->ship.state != RUNNING) {
        vTaskDelay(pdMS_TO_TICKS(200));
    }

    if (!systemRunning) {
        vTaskDelete(NULL);
        return;
    }

    printf(
        "[%s] Detecto entrada al canal | Posicion: %d\n",
        shipTask->taskName,
        shipTask->ship.position
    );

    while (systemRunning && shipTask->ship.state != FINISHED) {
        bool moved = canal_move_task(&demoCanal, shipTask);

        if (!moved) {
            printf(
                "[%s] No pudo moverse | Posicion actual: %d\n",
                shipTask->taskName,
                shipTask->ship.position
            );
        }

        vTaskDelay(pdMS_TO_TICKS(config.tickMs));
    }

    if (shipTask->ship.state == FINISHED) {
        printf(
            "[%s] Finalizo cruce | Estado: %s\n",
            shipTask->taskName,
            state_to_string(shipTask->ship.state)
        );

        vPortFree(shipTask);
    }

    vTaskDelete(NULL);
}

static void render_outputs(void)
{
    if (take_queues()) {
        hardware_render_state(
            &leftQueue,
            &rightQueue,
            &demoCanal,
            &flowPolicy
        );

        ui_bridge_send_state(
            &leftQueue,
            &rightQueue,
            &demoCanal,
            &flowPolicy
        );

        give_queues();
    }
}

static void trim_newline(char *line)
{
    if (line == NULL) {
        return;
    }

    size_t len = strlen(line);

    while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
        line[len - 1] = '\0';
        len--;
    }
}

static SchedulerType scheduler_from_string(const char *text)
{
    if (text == NULL) {
        return SCHED_FCFS;
    }

    if (strcmp(text, "FCFS") == 0) {
        return SCHED_FCFS;
    }

    if (strcmp(text, "SJF") == 0) {
        return SCHED_SJF;
    }

    if (strcmp(text, "STRN") == 0) {
        return SCHED_STRN;
    }

    if (strcmp(text, "Priority") == 0) {
        return SCHED_PRIORITY;
    }

    if (strcmp(text, "EDF") == 0) {
        return SCHED_EDF;
    }

    if (strcmp(text, "Round Robin") == 0) {
        return SCHED_RR;
    }

    return SCHED_FCFS;
}

static FLOWTYPE flow_from_string(const char *text)
{
    if (text == NULL) {
        return FLOW_SIGN;
    }

    if (strcmp(text, "Equidad") == 0) {
        return FLOW_FAIRNESS;
    }

    if (strcmp(text, "Letrero") == 0) {
        return FLOW_SIGN;
    }

    if (strcmp(text, "Tico") == 0) {
        return FLOW_TICO;
    }

    return FLOW_SIGN;
}

static ShipType ship_type_from_char(char typeChar)
{
    switch (typeChar) {
        case 'N':
        case 'n':
            return NORMAL;

        case 'F':
        case 'f':
            return FISHING;

        case 'P':
        case 'p':
            return PATROL;

        default:
            return NORMAL;
    }
}

static Direction direction_from_char(char sideChar)
{
    if (sideChar == 'R' || sideChar == 'r') {
        return RIGHT;
    }

    return LEFT;
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

static bool take_queues(void)
{
    if (queuesSemaphore == NULL) {
        return false;
    }

    return xSemaphoreTake(queuesSemaphore, portMAX_DELAY) == pdTRUE;
}

static void give_queues(void)
{
    if (queuesSemaphore == NULL) {
        return;
    }

    xSemaphoreGive(queuesSemaphore);
}