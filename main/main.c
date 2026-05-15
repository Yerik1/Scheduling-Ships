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
#define MOVEMENT_BASE_MS 1000
#define MOVEMENT_SUBTICK_MS 100
#define MOVEMENT_SETTLE_MS 20
#define SENSOR_RELEASE_CONFIRM_SUBTICKS 5

static int proximityReleaseCounter = 0;

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
static int visibleQueueSlots = 4;

static ShipTask *interruptedShips[PHYSICAL_CANAL_CELLS];
static int interruptedShipCount = 0;

static void simulation_task(void *params);
static void command_task(void *params);
static void ship_task_entry(void *params);
static void proximity_task(void *params);

static void wait_for_ui_start(void);
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
static const char *ship_type_to_string(ShipType type);
static const char *direction_to_string(Direction direction);
static const char *state_to_string(ShipState state);
static float get_length_speed_factor(void);
static float get_effective_ship_speed(ShipTask *shipTask);

static bool take_queues(void);
static void give_queues(void);
static bool is_proximity_safety_active(void);
static void handle_proximity_interrupt(void);
static void notify_ships_and_render(void);
static bool try_release_one_ship_to_canal(int *rrIndexLeft, int *rrIndexRight);
static bool fixed_simulation_finished(void);
static void reorder_queue_by_scheduler_unlocked(ReadyQueue *queue);
static void reorder_queues_by_scheduler_unlocked(void);
static bool restore_interrupted_ships_to_canal(void);
static void clear_interrupted_ships_buffer(void);


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

    /*
     * Importante:
     * Debe estar en true antes de recibir los GEN iniciales,
     * porque wait_for_ui_start() crea las ShipTasks.
     */

    systemRunning = true;

    wait_for_ui_start();

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

        config.signInterval = (signIntervalMs + MOVEMENT_BASE_MS - 1) / MOVEMENT_BASE_MS;

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
    if (strncmp(line, "CFG_VISIBLE:", 12) == 0)
    {
        visibleQueueSlots = atoi(line + 12);

        if (visibleQueueSlots <= 0)
        {
            visibleQueueSlots = 1;
        }

        if (visibleQueueSlots > 4)
        {
            visibleQueueSlots = 4;
        }

        if (visibleQueueSlots > queueCapacity)
        {
            visibleQueueSlots = queueCapacity;
        }

        hardware_set_visible_queue_slots(visibleQueueSlots);

        printf("CFG visibleQueueSlots=%d\n", visibleQueueSlots);
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

    render_outputs();

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
    if (proximitySafetyActive)
    {
        printf("Interrupcion ya activa. Ignorando nuevo trigger.\n");
        return;
    }

    printf("PROXIMITY SENSOR TRIGGERED: entrando en modo de seguridad. Bajando agujas y evacuando canal.\n");

    proximitySafetyActive = true;
    proximityReleaseCounter = 0;
    canal_block(&demoCanal);


    interruptedShipCount = 0;

    if (!canal_is_empty(&demoCanal))
    {
        ShipTask *tasks[PHYSICAL_CANAL_CELLS];
        int count = 0;

        for (int i = 0; i < demoCanal.length; i++)
        {
            ShipTask *task = demoCanal.ships_inside.tasks[i];

            if (task != NULL && count < PHYSICAL_CANAL_CELLS)
            {
                tasks[count++] = task;
            }
        }

        for (int i = 0; i < count; i++)
        {
            ShipTask *task = tasks[i];

            task->savedPosition = task->ship.position;
            task->savedMoveCredit = task->moveCredit;
            task->hasCheckpoint = true;
            task->justEntered = false;

            /*
             * Se retira temporalmente del canal.
             * No vuelve a la ready queue.
             */
            task->ship.state = READY;

            ShipTask *removedTask = canal_remove_task(&demoCanal, task);

            if (removedTask == NULL)
            {
                printf("ERROR: no se pudo remover barco %d del canal.\n", task->ship.id);
                continue;
            }

            if (interruptedShipCount < PHYSICAL_CANAL_CELLS)
            {
                interruptedShips[interruptedShipCount++] = removedTask;

                printf(
                    "Barco %d retirado por interrupcion | savedPosition=%d | savedCredit=%.2f\n",
                    removedTask->ship.id,
                    removedTask->savedPosition,
                    removedTask->savedMoveCredit
                );
            }
            else
            {
                printf("ERROR: buffer de barcos interrumpidos lleno.\n");
            }
        }
    }

    printf("Total barcos interrumpidos guardados: %d\n", interruptedShipCount);

    render_outputs();

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

        if (!is_proximity_safety_active())
        {
            handle_proximity_interrupt();
        }
        else
        {
            printf("ProximityTask: interrupcion ignorada porque ya esta activa.\n");
        }
    }

    vTaskDelete(NULL);
}

static bool restore_interrupted_ships_to_canal(void)
{
    if (interruptedShipCount <= 0)
    {
        return true;
    }

    printf("Restaurando %d barcos interrumpidos al canal...\n", interruptedShipCount);

    if (!canal_is_empty(&demoCanal))
    {
        printf("No se puede restaurar: el canal no esta vacio.\n");
        return false;
    }

    for (int i = 0; i < interruptedShipCount; i++)
    {
        ShipTask *task = interruptedShips[i];

        if (task == NULL)
        {
            continue;
        }

        bool entered = canal_enter_at(
            &demoCanal,
            task,
            task->savedPosition
        );

        if (!entered)
        {
            printf(
                "ERROR: barco %d no pudo restaurarse en posicion %d\n",
                task->ship.id,
                task->savedPosition
            );

            return false;
        }

        task->moveCredit = task->savedMoveCredit;
        task->hasCheckpoint = false;
        task->savedPosition = 0;
        task->savedMoveCredit = 0.0f;
        task->justEntered = false;
        task->hasEnteredBefore = true;

        printf(
            "Barco %d restaurado en posicion %d\n",
            task->ship.id,
            task->ship.position
        );
    }

    clear_interrupted_ships_buffer();

    return true;
}

static void clear_interrupted_ships_buffer(void)
{
    for (int i = 0; i < PHYSICAL_CANAL_CELLS; i++)
    {
        interruptedShips[i] = NULL;
    }

    interruptedShipCount = 0;
}

static void simulation_task(void *params)
{
    (void)params;

    int subtick = 0;
    int rrIndexLeft = 0;
    int rrIndexRight = 0;
    int elapsedBaseMs = 0;

    printf("SimulationTask iniciada.\n\n");

    while (systemRunning)
    {
        subtick++;

        proximity_sensor_trigger_ping();

        if (hardware_sensor_active() && !is_proximity_safety_active())
        {
            handle_proximity_interrupt();

            /*
             * IMPORTANTE:
             * handle_proximity_interrupt() limpia el interrupt al final.
             * Si seguimos en este mismo subtick, el sistema puede creer
             * que el sensor ya se liberó y levantar la aguja inmediatamente.
             */
            render_outputs();
            vTaskDelay(pdMS_TO_TICKS(MOVEMENT_SUBTICK_MS));
            continue;
        }

        if (is_proximity_safety_active())
        {
            bool sensorDetectedThisSubtick = hardware_sensor_active();

            if (sensorDetectedThisSubtick)
            {
                proximityReleaseCounter = 0;
            }
            else
            {
                proximityReleaseCounter++;

                printf(
                    "Sensor sin deteccion | contador liberacion: %d/%d\n",
                    proximityReleaseCounter,
                    SENSOR_RELEASE_CONFIRM_SUBTICKS
                );
            }

            /*
             * IMPORTANTE:
             * Ya usamos la lectura de este subtick.
             * Ahora limpiamos el latch para que el próximo
             * proximity_sensor_trigger_ping() actualice el estado real.
             */
            hardware_clear_interrupt();

            if (proximityReleaseCounter >= SENSOR_RELEASE_CONFIRM_SUBTICKS)
            {
                printf("Sensor liberado estable. Intentando restaurar barcos interrumpidos.\n");

                canal_unblock(&demoCanal);

                if (restore_interrupted_ships_to_canal())
                {
                    printf("Modo de seguridad finalizado. Barcos restaurados. Levantando agujas.\n");

                    proximitySafetyActive = false;
                    proximityReleaseCounter = 0;

                    render_outputs();

                    vTaskDelay(pdMS_TO_TICKS(300));
                }
                else
                {
                    printf("No se pudo restaurar todavia. Manteniendo modo seguridad.\n");

                    canal_block(&demoCanal);
                    proximityReleaseCounter = 0;
                }
            }

            render_outputs();
            vTaskDelay(pdMS_TO_TICKS(MOVEMENT_SUBTICK_MS));
            continue;
        }

        /*
         * La politica de flujo avanza cada tick base, no cada subtick.
         */
        elapsedBaseMs += MOVEMENT_SUBTICK_MS;

        if (elapsedBaseMs >= MOVEMENT_BASE_MS)
        {
            elapsedBaseMs = 0;
            flow_policy_on_tick(&flowPolicy);

            printf("\n---------- BASE TICK ----------\n");
            printf(
                "FlowPolicy: %s | Direccion actual: %s\n",
                flow_type_to_string(flowPolicy.type),
                flow_decision_to_string(flowPolicy.direction));
        }

        /*
         * 1. Intentar meter barco antes del movimiento.
         */
        try_release_one_ship_to_canal(&rrIndexLeft, &rrIndexRight);

        /*
         * 2. Mover barcos dentro del canal.
         * Cada ShipTask suma credito de subtick e intenta maximo 1 paso.
         */
        notify_ships_and_render();

        /*
         * 3. Intentar meter otro barco después del movimiento.
         * Esto evita espacios artificiales con pesqueros y patrullas.
         */
        try_release_one_ship_to_canal(&rrIndexLeft, &rrIndexRight);

        /*
         * 4. Render final del subtick.
         */
        render_outputs();

        if (fixed_simulation_finished())
        {
            printf("\nNo quedan barcos en colas ni en canal. Simulacion fija finalizada.\n");
            break;
        }

        vTaskDelay(pdMS_TO_TICKS(MOVEMENT_SUBTICK_MS));
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
                "[%s] WARN: desperto con checkpoint activo. No se limpia aqui.\n",
                shipTask->taskName
            );
        }
        else
        {
            printf(
                "[%s] Entro al canal | Posicion: %d\n",
                shipTask->taskName,
                shipTask->ship.position
            );
        }

        bool firstMovementAfterEntry = !shipTask->hasEnteredBefore;
        shipTask->hasEnteredBefore = true;

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

            if (shipTask->justEntered)
            {
                /*
                 * Al entrar al canal, no le damos un movimiento completo.
                 * Solo reiniciamos el crédito para que empiece a acumular
                 * de forma normal con los subticks.
                 */
                shipTask->justEntered = false;
                shipTask->moveCredit = 0.0f;

                printf(
                    "[%s] Primer subtick tras entrada | Posicion: %d\n",
                    shipTask->taskName,
                    shipTask->ship.position
                );
            }

            /*
             * En subticks NO se suma effectiveSpeed completo.
             * Se suma solo la fracción correspondiente al intervalo del subtick.
             */
            float creditIncrement =
                shipTask->effectiveSpeed *
                ((float)MOVEMENT_SUBTICK_MS / 1000.0f);

            shipTask->moveCredit += creditIncrement;

            bool movedAtLeastOnce = false;
            bool blocked = false;

            /*
             * En subtick, intenta como máximo 1 paso unitario.
             * Así nunca se ve como salto.
             */
            if (
                shipTask->moveCredit >= 1.0f &&
                shipTask->ship.state != FINISHED &&
                systemRunning
            ) {
                bool moved = canal_move_one_step(&demoCanal, shipTask);

                if (moved) {
                    movedAtLeastOnce = true;
                    shipTask->moveCredit -= 1.0f;
                } else {
                    blocked = true;
                    shipTask->moveCredit = 0.0f;
                }
            }

            if (!movedAtLeastOnce && !blocked && shipTask->moveCredit < 1.0f) {
                printf(
                    "[%s] Acumulando credito | Credito: %.2f | Incremento: %.2f | Velocidad efectiva: %.2f\n",
                    shipTask->taskName,
                    shipTask->moveCredit,
                    creditIncrement,
                    shipTask->effectiveSpeed
                );
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
        /*
         * Reordena antes de mostrar en HW/UI.
         * Así lo visible coincide con el scheduler.
         */
        reorder_queues_by_scheduler_unlocked();

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
        return SCHD_RR;
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

#define MOVEMENT_SETTLE_MS 20

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

static bool try_release_one_ship_to_canal(int *rrIndexLeft, int *rrIndexRight)
{
    if (is_proximity_safety_active()) {
        return false;
    }

    if (!take_queues()) {
        return false;
    }

    reorder_queues_by_scheduler_unlocked();

    FLOWDECISION decision = flow_policy_select_side(
        &flowPolicy,
        &leftQueue,
        &rightQueue
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
        give_queues();
        return false;
    }

    int selectedIndex = -1;

    if (config.schedulerType == SCHD_RR) {
        if (releasedSide == FLOW_LEFT) {
            selectedIndex = scheduler_rr(selectedQueue, config.rrQuantum, rrIndexLeft);
        } else {
            selectedIndex = scheduler_rr(selectedQueue, config.rrQuantum, rrIndexRight);
        }
    } else {
        selectedIndex = select_scheduler_index(config.schedulerType, selectedQueue);
    }

    if (selectedIndex < 0) {
        give_queues();
        return false;
    }

    ShipTask *selectedTask = queue_get(selectedQueue, selectedIndex);

    if (selectedTask == NULL) {
        give_queues();
        return false;
    }

    bool entered = canal_enter(&demoCanal, selectedTask);

    if (entered) {
        printf(
            "Barco %d entro exitosamente desde %s\n",
            selectedTask->ship.id,
            flow_decision_to_string(releasedSide)
        );

        selectedTask->justEntered = true;

        queue_remove(selectedQueue, selectedIndex);
        flow_policy_on_ship_released(&flowPolicy, releasedSide);
    }

    give_queues();

    return entered;
}

static bool fixed_simulation_finished(void)
{
    bool finished = false;

    if (!take_queues()) {
        return false;
    }

    finished =
        !dynamicGenerationEnabled &&
        queue_is_empty(&leftQueue) &&
        queue_is_empty(&rightQueue) &&
        canal_is_empty(&demoCanal);

    give_queues();

    return finished;
}

static void reorder_queue_by_scheduler_unlocked(ReadyQueue *queue)
{
    if (queue == NULL) {
        return;
    }

    /*
     * FCFS ya representa orden de llegada.
     * RR no se debe reordenar porque depende de su índice rotativo.
     */
    if (config.schedulerType == SCHED_FCFS || config.schedulerType == SCHD_RR) {
        return;
    }

    if (queue->count <= 1) {
        return;
    }

    ReadyQueue workQueue;
    ReadyQueue sortedQueue;

    queue_init(&workQueue);
    queue_init(&sortedQueue);

    for (int i = 0; i < queue->count; i++) {
        workQueue.tasks[i] = queue->tasks[i];
    }

    workQueue.count = queue->count;

    while (!queue_is_empty(&workQueue)) {
        int selectedIndex = select_scheduler_index(config.schedulerType, &workQueue);

        if (selectedIndex < 0) {
            break;
        }

        ShipTask *selectedTask = queue_get(&workQueue, selectedIndex);

        if (selectedTask == NULL) {
            break;
        }

        queue_add(&sortedQueue, selectedTask);
        queue_remove(&workQueue, selectedIndex);
    }

    queue->count = sortedQueue.count;

    for (int i = 0; i < sortedQueue.count; i++) {
        queue->tasks[i] = sortedQueue.tasks[i];
    }
}

static void reorder_queues_by_scheduler_unlocked(void)
{
    reorder_queue_by_scheduler_unlocked(&leftQueue);
    reorder_queue_by_scheduler_unlocked(&rightQueue);
}

