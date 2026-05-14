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
#define PROXIMITY_TASK_PRIORITY (SIMULATION_TASK_PRIORITY + 1)
#define MOVEMENT_SUBSTEP_MS 250

static AppConfig config;

static Canal demoCanal;
static ReadyQueue leftQueue;
static ReadyQueue rightQueue;
static FlowPolicy flowPolicy;

static TaskHandle_t simulationTaskHandle = NULL;
static TaskHandle_t commandTaskHandle = NULL;
static TaskHandle_t proximityTaskHandle = NULL;

static SemaphoreHandle_t queuesSemaphore = NULL;

static volatile bool systemRunning = false;
static bool dynamicGenerationEnabled = false;
static bool proximitySafetyActive = false;
static int queueCapacity = 4;

static void simulation_task(void *params);
static void command_task(void *params);
static void ship_task_entry(void *params);
static void proximity_task(void *params);

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
static float get_length_speed_factor(void);
static float get_effective_ship_speed(ShipTask *shipTask);

static bool take_queues(void);
static void give_queues(void);
static bool configReceived = false;
static bool is_proximity_safety_active(void);
static void handle_proximity_interrupt(void);
static void notify_ships_and_render(void);
static int get_substep_delay_ms(ShipTask *shipTask, int stepsThisTick);

void app_main(void)
{
    printf("\n\n===============================\n");
    printf("Scheduling Ships - UI Mode\n");
    printf("===============================\n\n");

    config_load_defaults(&config);

    queue_init(&leftQueue);
    queue_init(&rightQueue);

    queuesSemaphore = xSemaphoreCreateBinary();

    if (queuesSemaphore == NULL)
    {
        printf("ERROR: No se pudo crear queuesSemaphore\n");
        return;
    }

    xSemaphoreGive(queuesSemaphore);

    bool hardwareOk = hardware_init();

    if (!hardwareOk)
    {
        printf("ADVERTENCIA: Hardware no inicializado correctamente. La simulacion continua.\n");
        hardware_set_enabled(false);
    }

    if (!serial_comm_init())
    {
        printf("ERROR: No se pudo inicializar serial_comm\n");
        return;
    }

    wait_for_ui_start();

    systemRunning = true;

    BaseType_t proxResult = xTaskCreate(
        proximity_task,
        "ProximityTask",
        SHIP_TASK_STACK_SIZE,
        NULL,
        PROXIMITY_TASK_PRIORITY,
        &proximityTaskHandle);

    if (proxResult != pdPASS)
    {
        printf("ERROR: No se pudo crear ProximityTask\n");
    }
    else
    {
        proximity_sensor_set_notify_task(proximityTaskHandle);
    }

    render_outputs();

    BaseType_t simResult = xTaskCreate(
        simulation_task,
        "SimulationTask",
        SIMULATION_TASK_STACK_SIZE,
        NULL,
        SIMULATION_TASK_PRIORITY,
        &simulationTaskHandle);

    if (simResult != pdPASS)
    {
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
        &commandTaskHandle);

    if (cmdResult != pdPASS)
    {
        printf("ADVERTENCIA: No se pudo crear CommandTask. No habra generacion dinamica.\n");
    }
}

static bool process_cfg_command(char *line)
{
    if (strncmp(line, "CFG_LEN:", 8) == 0)
    {
        config.canalLength = atoi(line + 8);

        if (config.canalLength <= 0)
        {
            config.canalLength = PHYSICAL_CANAL_CELLS;
        }

        printf("CFG canalLength logico=%d\n", config.canalLength);
        return true;
    }

    if (strncmp(line, "CFG_Q:", 6) == 0)
    {
        queueCapacity = atoi(line + 6);

        if (queueCapacity <= 0)
        {
            queueCapacity = 1;
        }

        if (queueCapacity > READY_QUEUE_MAX_CAPACITY)
        {
            queueCapacity = READY_QUEUE_MAX_CAPACITY;
        }

        printf("CFG queueCapacity=%d\n", queueCapacity);
        return true;
    }

    if (strncmp(line, "CFG_S:", 6) == 0)
    {
        config.schedulerType = scheduler_from_string(line + 6);
        printf("CFG scheduler=%s\n", line + 6);
        return true;
    }

    if (strncmp(line, "CFG_F:", 6) == 0)
    {
        config.flowType = flow_from_string(line + 6);
        printf("CFG flow=%s\n", line + 6);
        return true;
    }

    if (strncmp(line, "CFG_W:", 6) == 0)
    {
        config.fairnessW = atoi(line + 6);

        if (config.fairnessW <= 0)
        {
            config.fairnessW = 1;
        }

        printf("CFG fairnessW=%d\n", config.fairnessW);
        return true;
    }

    if (strncmp(line, "CFG_SIGN:", 9) == 0)
    {
        int signIntervalMs = atoi(line + 9);

        if (config.tickMs <= 0)
        {
            config.tickMs = 500;
        }

        config.signInterval = (signIntervalMs + config.tickMs - 1) / config.tickMs;

        if (config.signInterval <= 0)
        {
            config.signInterval = 1;
        }

        printf("CFG signInterval ticks=%d\n", config.signInterval);
        return true;
    }

    if (strncmp(line, "CFG_RR:", 7) == 0)
    {
        config.rrQuantum = atoi(line + 7);

        if (config.rrQuantum <= 0)
        {
            config.rrQuantum = 1;
        }

        printf("CFG rrQuantum=%d\n", config.rrQuantum);
        return true;
    }

    if (strncmp(line, "CFG_MODE:", 9) == 0)
    {
        char *mode = line + 9;

        dynamicGenerationEnabled = false;

        if (strncmp(mode, "Din", 3) == 0)
        {
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

    while (!started)
    {
        int len = serial_comm_read_line(line, sizeof(line));

        if (len <= 0)
        {
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }

        printf("RX UI: %s\n", line);

        if (strcmp(line, "PING") == 0)
        {
            serial_comm_write_line("ACK:PING");
            continue;
        }

        if (strncmp(line, "CFG_", 4) == 0)
        {
            process_cfg_command(line);
            serial_comm_write_line("ACK:CFG");
            continue;
        }

        if (strncmp(line, "GEN:", 4) == 0)
        {
            char copy[UI_LINE_BUFFER_SIZE];
            strncpy(copy, line, sizeof(copy));
            copy[sizeof(copy) - 1] = '\0';

            process_gen_command(copy, true);
            serial_comm_write_line("ACK:GEN");
            continue;
        }

        if (strcmp(line, "START") == 0)
        {
            canal_init(&demoCanal, PHYSICAL_CANAL_CELLS, LEFT);

            flow_policy_init(
                &flowPolicy,
                &demoCanal,
                FLOW_LEFT,
                config.flowType,
                config.fairnessW,
                config.signInterval);

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

    if (token == NULL || strcmp(token, "GEN") != 0)
    {
        return false;
    }

    char *sideStr = strtok(NULL, ":");
    char *typeStr = strtok(NULL, ":");

    if (sideStr == NULL || typeStr == NULL)
    {
        printf("ERROR: GEN incompleto\n");
        return false;
    }

    if (!isInitialLoad && !dynamicGenerationEnabled)
    {
        printf("Modo fijo activo. GEN ignorado: %s:%s\n", sideStr, typeStr);
        return false;
    }

    return create_ship_from_ui(sideStr[0], typeStr[0]);
}

static float get_length_speed_factor(void)
{
    if (config.canalLength <= 0)
    {
        return 1.0f;
    }

    return (float)PHYSICAL_CANAL_CELLS / (float)config.canalLength;
}

static float get_effective_ship_speed(ShipTask *shipTask)
{
    if (shipTask == NULL)
    {
        return 0.0f;
    }

    return (float)getSpeed(&shipTask->ship) * get_length_speed_factor();
}

static bool create_ship_from_ui(char sideChar, char typeChar)
{
    Direction direction = direction_from_char(sideChar);
    ShipType type = ship_type_from_char(typeChar);

    ShipTask *shipTask = pvPortMalloc(sizeof(ShipTask));

    if (shipTask == NULL)
    {
        printf("ERROR: No se pudo reservar memoria para ShipTask\n");
        return false;
    }

    memset(shipTask, 0, sizeof(ShipTask));

    shipTask->ship = createShip(type, direction, PHYSICAL_CANAL_CELLS);
    shipTask->handle = NULL;
    shipTask->maxSteps = PHYSICAL_CANAL_CELLS + 5;

    shipTask->effectiveSpeed = get_effective_ship_speed(shipTask);
    shipTask->moveCredit = 0.0f;

    snprintf(
        shipTask->taskName,
        sizeof(shipTask->taskName),
        "Ship_%d",
        shipTask->ship.id);

    BaseType_t result = xTaskCreate(
        ship_task_entry,
        shipTask->taskName,
        SHIP_TASK_STACK_SIZE,
        shipTask,
        SHIP_TASK_PRIORITY,
        &shipTask->handle);

    if (result != pdPASS)
    {
        printf("ERROR: No se pudo crear task para barco %d\n", shipTask->ship.id);
        vPortFree(shipTask);
        return false;
    }

    ReadyQueue *targetQueue = direction == LEFT ? &leftQueue : &rightQueue;

    if (!take_queues())
    {
        printf("ERROR: No se pudo tomar semaforo de colas\n");
        vTaskDelete(shipTask->handle);
        vPortFree(shipTask);
        return false;
    }

    if (targetQueue->count >= queueCapacity)
    {
        printf(
            "ERROR: Cola %s llena. No se pudo agregar barco %d\n",
            direction_to_string(direction),
            shipTask->ship.id);

        give_queues();

        vTaskDelete(shipTask->handle);
        vPortFree(shipTask);

        return false;
    }

    if (!queue_add(targetQueue, shipTask))
    {
        printf("ERROR: queue_add fallo para barco %d\n", shipTask->ship.id);

        give_queues();

        vTaskDelete(shipTask->handle);
        vPortFree(shipTask);

        return false;
    }

    give_queues();

    printf(
        "Barco creado desde UI | ID: %d | Lado: %c | Tipo: %c | BaseSpeed: %d | EffectiveSpeed: %.2f\n",
        shipTask->ship.id,
        sideChar,
        typeChar,
        getSpeed(&shipTask->ship),
        shipTask->effectiveSpeed);

    return true;
}

static bool is_proximity_safety_active(void)
{
    return proximitySafetyActive;
}

static void handle_proximity_interrupt(void)
{
    printf("PROXIMITY SENSOR TRIGGERED: entrando en modo de seguridad. Bajando agujas y evacuando canal.\n");

    proximitySafetyActive = true;
    canal_block(&demoCanal);

    if (!canal_is_empty(&demoCanal))
    {
        ShipTask *tasks[CANAL_LEN];
        int count = 0;

        for (int i = 0; i < demoCanal.length; i++)
        {
            ShipTask *task = demoCanal.ships_inside.tasks[i];

            if (task != NULL)
            {
                tasks[count++] = task;
            }
        }

        if (!take_queues())
        {
            printf("ERROR: no se pudo tomar semaforo de colas para reencolar barcos del canal.\n");
        }
        else
        {
            for (int i = 0; i < count; i++)
            {
                ShipTask *task = tasks[i];

                // Save Ship Position
                task->savedPosition = task->ship.position;
                task->hasCheckpoint = true;

                task->ship.state = READY;
                ShipTask *removedTask = canal_remove_task(&demoCanal, task);

                if (removedTask == NULL)
                {
                    printf("ERROR: no se pudo remover barco %d del canal.\n", task->ship.id);
                    continue;
                }

                ReadyQueue *targetQueue = removedTask->ship.origin == LEFT ? &leftQueue : &rightQueue;

                if (targetQueue->count >= queueCapacity)
                {
                    printf(
                        "ERROR: cola %s llena al reingresar barco %d. El barco queda READY pero no se reencola.\n",
                        direction_to_string(removedTask->ship.origin),
                        removedTask->ship.id);
                    continue;
                }

                if (!queue_add(targetQueue, removedTask))
                {
                    printf(
                        "ERROR: no se pudo reencolar barco %d luego de removerlo del canal.\n",
                        removedTask->ship.id);
                }
                else
                {
                    printf(
                        "Barco %d removido del canal y reencolado en %s.\n",
                        removedTask->ship.id,
                        direction_to_string(removedTask->ship.origin));
                }
            }

            give_queues();
        }
    }

    hardware_clear_interrupt();
}

static void proximity_task(void *params)
{
    (void)params;

    while (systemRunning)
    {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        if (!systemRunning)
        {
            break;
        }

        handle_proximity_interrupt();
    }

    vTaskDelete(NULL);
}

static void simulation_task(void *params)
{
    (void)params;

    int tick = 0;
    int rrIndexLeft = 0;
    int rrIndexRight = 0;

    printf("SimulationTask iniciada.\n\n");

    while (systemRunning)
    {
        tick++;

        proximity_sensor_trigger_ping();

        printf("\n---------- TICK %d ----------\n", tick);

        if (hardware_sensor_active() && !is_proximity_safety_active())
        {
            handle_proximity_interrupt();
        }

        if (is_proximity_safety_active())
        {
            if (!hardware_sensor_active() && canal_is_empty(&demoCanal))
            {
                printf("Modo de seguridad finalizado. Levantando agujas.\n");
                canal_unblock(&demoCanal);
                proximitySafetyActive = false;
            }
        }

        flow_policy_on_tick(&flowPolicy);

        if (!take_queues())
        {
            printf("No se pudo tomar semaforo de colas en SimulationTask.\n");
            render_outputs();
            vTaskDelay(pdMS_TO_TICKS(config.tickMs));
            continue;
        }

        FLOWDECISION decision = flow_policy_select_side(
            &flowPolicy,
            &leftQueue,
            &rightQueue);

        printf(
            "FlowPolicy: %s | Direccion actual: %s | Decision: %s\n",
            flow_type_to_string(flowPolicy.type),
            flow_decision_to_string(flowPolicy.direction),
            flow_decision_to_string(decision));

        ReadyQueue *selectedQueue = NULL;
        FLOWDECISION releasedSide = FLOW_NONE;

        if (decision == FLOW_LEFT)
        {
            selectedQueue = &leftQueue;
            releasedSide = FLOW_LEFT;
        }
        else if (decision == FLOW_RIGHT)
        {
            selectedQueue = &rightQueue;
            releasedSide = FLOW_RIGHT;
        }

        if (selectedQueue == NULL || queue_is_empty(selectedQueue))
        {
            printf("No se libero ningun barco este tick.\n");

            bool fixedFinished =
                !dynamicGenerationEnabled &&
                queue_is_empty(&leftQueue) &&
                queue_is_empty(&rightQueue) &&
                canal_is_empty(&demoCanal);

            give_queues();

            notify_ships_and_render();

            if (fixedFinished)
            {
                printf("\nNo quedan barcos en colas ni en canal. Simulacion fija finalizada.\n");
                break;
            }

            vTaskDelay(pdMS_TO_TICKS(config.tickMs));
            continue;
        }

        int selectedIndex = -1;

        if (config.schedulerType == SCHD_RR)
        {
            if (releasedSide == FLOW_LEFT)
            {
                selectedIndex = scheduler_rr(selectedQueue, config.rrQuantum, &rrIndexLeft);
            }
            else
            {
                selectedIndex = scheduler_rr(selectedQueue, config.rrQuantum, &rrIndexRight);
            }
        }
        else
        {
            selectedIndex = select_scheduler_index(config.schedulerType, selectedQueue);
        }

        if (selectedIndex < 0)
        {
            printf("Scheduler no selecciono ningun barco.\n");
            give_queues();
            notify_ships_and_render();
            vTaskDelay(pdMS_TO_TICKS(config.tickMs));
            continue;
        }

        ShipTask *selectedTask = queue_get(selectedQueue, selectedIndex);

        if (selectedTask == NULL)
        {
            printf("La task seleccionada es NULL.\n");
            give_queues();
            notify_ships_and_render();
            vTaskDelay(pdMS_TO_TICKS(config.tickMs));
            continue;
        }

        printf(
            "Scheduler selecciono barco %d | Tipo: %s | Origen: %s | Pos: %d\n",
            selectedTask->ship.id,
            ship_type_to_string(selectedTask->ship.type),
            direction_to_string(selectedTask->ship.origin),
            selectedTask->ship.position);

        if (canal_enter(&demoCanal, selectedTask))
        {
            printf(
                "Barco %d entro exitosamente desde %s\n",
                selectedTask->ship.id,
                flow_decision_to_string(releasedSide));

            queue_remove(selectedQueue, selectedIndex);
            flow_policy_on_ship_released(&flowPolicy, releasedSide);
        }
        else
        {
            printf(
                "Barco %d no pudo entrar al canal todavia.\n",
                selectedTask->ship.id);
        }

        bool fixedFinished =
            !dynamicGenerationEnabled &&
            queue_is_empty(&leftQueue) &&
            queue_is_empty(&rightQueue) &&
            canal_is_empty(&demoCanal);

        give_queues();

        notify_ships_and_render();

        if (fixedFinished)
        {
            printf("\nNo quedan barcos en colas ni en canal. Simulacion fija finalizada.\n");
            break;
        }

        vTaskDelay(pdMS_TO_TICKS(config.tickMs));
    }

    systemRunning = false;

    notify_ships_and_render();

    printf("\nSimulationTask finalizada.\n");

    vTaskDelete(NULL);
}

static void command_task(void *params)
{
    (void)params;

    char line[UI_LINE_BUFFER_SIZE];

    printf("CommandTask iniciada. Esperando comandos dinamicos.\n");

    while (systemRunning)
    {
        int len = serial_comm_read_line(line, sizeof(line));

        if (len <= 0)
        {
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }

        trim_newline(line);

        if (strlen(line) == 0)
        {
            continue;
        }

        printf("RX UI runtime: %s\n", line);

        if (strncmp(line, "GEN:", 4) == 0)
        {
            char copy[UI_LINE_BUFFER_SIZE];
            strncpy(copy, line, sizeof(copy));
            copy[sizeof(copy) - 1] = '\0';

            if (process_gen_command(copy, false))
            {
                serial_comm_write_line("ACK:GEN");
            }
            else
            {
                serial_comm_write_line("ERR:GEN");
            }

            render_outputs();
        }
        else if (strcmp(line, "STOP") == 0)
        {
            printf("STOP recibido desde UI.\n");
            systemRunning = false;
            break;
        }
        else
        {
            printf("Comando runtime ignorado: %s\n", line);
        }
    }

    printf("CommandTask finalizada.\n");
    vTaskDelete(NULL);
}

static void ship_task_entry(void *params)
{
    ShipTask *shipTask = (ShipTask *)params;

    if (shipTask == NULL)
    {
        vTaskDelete(NULL);
        return;
    }

    printf(
        "[%s] Task iniciada | ID: %d | Tipo: %s | Origen: %s | Estado: %s | EffectiveSpeed: %.2f\n",
        shipTask->taskName,
        shipTask->ship.id,
        ship_type_to_string(shipTask->ship.type),
        direction_to_string(shipTask->ship.origin),
        state_to_string(shipTask->ship.state),
        shipTask->effectiveSpeed);

    while (systemRunning)
    {
        while (systemRunning && shipTask->ship.state != RUNNING)
        {
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        }

        if (!systemRunning)
            break;

        if (shipTask->hasCheckpoint)
        {
            printf(
                "[%s] Reanudando desde checkpoint | Posicion guardada: %d\n",
                shipTask->taskName,
                shipTask->savedPosition);

            canal_set_ship_position(&demoCanal, shipTask, shipTask->savedPosition);

            shipTask->hasCheckpoint = false;
            shipTask->savedPosition = 0;
        }
        else
        {
            printf(
                "[%s] Entro al canal | Posicion: %d\n",
                shipTask->taskName,
                shipTask->ship.position);
        }

        bool firstMovementAfterEntry = true;

        while (systemRunning && shipTask->ship.state != FINISHED)
        {

            /*
             * La primera vez no espera otra notificación.
             * Usa la misma notificación que recibió al entrar al canal.
             */
            if (!firstMovementAfterEntry)
            {
                ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
            }
            else
            {
                firstMovementAfterEntry = false;
            }

            if (!systemRunning)
            {
                break;
            }

            if (shipTask->ship.state == READY)
            {
                break;
            }

            if (shipTask->ship.state != RUNNING)
            {
                continue;
            }

            shipTask->moveCredit += shipTask->effectiveSpeed;

            bool movedAtLeastOnce = false;
            bool blocked = false;

            /*
             * Cantidad máxima de pasos unitarios que puede intentar este barco
             * en este tick según su crédito acumulado.
             */
            int stepsThisTick = (int)shipTask->moveCredit;

            if (stepsThisTick < 1)
            {
                stepsThisTick = 0;
            }

            int substepDelayMs = get_substep_delay_ms(shipTask, stepsThisTick);

            while (
                shipTask->moveCredit >= 1.0f &&
                shipTask->ship.state != FINISHED &&
                systemRunning)
            {
                bool moved = canal_move_one_step(&demoCanal, shipTask);

                if (moved)
                {
                    movedAtLeastOnce = true;
                    shipTask->moveCredit -= 1.0f;

                    render_outputs();

                    /*
                     * Divide el tick entre los pasos del barco.
                     * Ej:
                     * speed 2 -> 500 ms
                     * speed 3 -> 333 ms
                     */
                    vTaskDelay(pdMS_TO_TICKS(substepDelayMs));
                }
                else
                {
                    blocked = true;
                    shipTask->moveCredit = 0.0f;
                    break;
                }
            }

            if (!movedAtLeastOnce && !blocked && shipTask->moveCredit < 1.0f)
            {
                printf(
                    "[%s] Acumulando credito | Credito: %.2f | Velocidad efectiva: %.2f\n",
                    shipTask->taskName,
                    shipTask->moveCredit,
                    shipTask->effectiveSpeed);
            }
        }

        if (shipTask->ship.state == FINISHED)
            break;
    }

    if (shipTask->ship.state == FINISHED)
    {
        printf(
            "[%s] Finalizo cruce | Estado: %s\n",
            shipTask->taskName,
            state_to_string(shipTask->ship.state));

        vPortFree(shipTask);
    }

    vTaskDelete(NULL);
}

static void render_outputs(void)
{
    if (take_queues())
    {
        hardware_render_state(
            &leftQueue,
            &rightQueue,
            &demoCanal,
            &flowPolicy);

        ui_bridge_send_state(
            &leftQueue,
            &rightQueue,
            &demoCanal,
            &flowPolicy);

        give_queues();
    }
}

static void trim_newline(char *line)
{
    if (line == NULL)
    {
        return;
    }

    size_t len = strlen(line);

    while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
    {
        line[len - 1] = '\0';
        len--;
    }
}

static SchedulerType scheduler_from_string(const char *text)
{
    if (text == NULL)
    {
        return SCHED_FCFS;
    }

    if (strcmp(text, "FCFS") == 0)
    {
        return SCHED_FCFS;
    }

    if (strcmp(text, "SJF") == 0)
    {
        return SCHED_SJF;
    }

    if (strcmp(text, "STRN") == 0)
    {
        return SCHED_STRN;
    }

    if (strcmp(text, "Priority") == 0)
    {
        return SCHED_PRIORITY;
    }

    if (strcmp(text, "EDF") == 0)
    {
        return SCHED_EDF;
    }

    if (strcmp(text, "Round Robin") == 0)
    {
        return SCHED_RR;
    }

    return SCHED_FCFS;
}

static FLOWTYPE flow_from_string(const char *text)
{
    if (text == NULL)
    {
        return FLOW_SIGN;
    }

    if (strcmp(text, "Equidad") == 0)
    {
        return FLOW_FAIRNESS;
    }

    if (strcmp(text, "Letrero") == 0)
    {
        return FLOW_SIGN;
    }

    if (strcmp(text, "Tico") == 0)
    {
        return FLOW_TICO;
    }

    return FLOW_SIGN;
}

static ShipType ship_type_from_char(char typeChar)
{
    switch (typeChar)
    {
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
    if (sideChar == 'R' || sideChar == 'r')
    {
        return RIGHT;
    }

    return LEFT;
}

static int select_scheduler_index(SchedulerType schedulerType, ReadyQueue *queue)
{
    switch (schedulerType)
    {
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
    switch (decision)
    {
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
    switch (type)
    {
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

static const char *ship_type_to_string(ShipType type)
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

static const char *direction_to_string(Direction direction)
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

static const char *state_to_string(ShipState state)
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

static bool take_queues(void)
{
    if (queuesSemaphore == NULL)
    {
        return false;
    }

    return xSemaphoreTake(queuesSemaphore, portMAX_DELAY) == pdTRUE;
}

static void give_queues(void)
{
    if (queuesSemaphore == NULL)
    {
        return;
    }

    xSemaphoreGive(queuesSemaphore);
}

#define MOVEMENT_SETTLE_MS 100

static void notify_ships_and_render(void)
{
    /*
     * Notificar en orden evita huecos:
     * primero se mueve el barco de adelante,
     * luego el que viene detrás.
     */
    canal_notify_ships_ordered(&demoCanal);

    vTaskDelay(pdMS_TO_TICKS(MOVEMENT_SETTLE_MS));

    render_outputs();
}

static int get_substep_delay_ms(ShipTask *shipTask, int stepsThisTick)
{
    if (shipTask == NULL || stepsThisTick <= 0)
    {
        return config.tickMs;
    }

    int delayMs = config.tickMs / stepsThisTick;

    if (delayMs < 50)
    {
        delayMs = 50;
    }

    return delayMs;
}