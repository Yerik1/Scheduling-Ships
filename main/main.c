#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

/*
 * FreeRTOS:
 * - task.h: creación y manejo de tareas.
 * - semphr.h: semáforos para proteger recursos compartidos.
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

/*
 * Módulos propios del proyecto.
 */
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

/*
 * Permite reiniciar la ESP con esp_restart()
 * cuando la UI envía STOP.
 */
#include "esp_system.h"


// ======================================================
// CONSTANTES GENERALES DEL SISTEMA
// ======================================================

/*
 * Tamaño máximo de línea recibida desde la interfaz.
 * Ejemplos:
 *   CFG_LEN:6
 *   GEN:L:N
 *   START
 */
#define UI_LINE_BUFFER_SIZE 160

/*
 * Configuración de tareas FreeRTOS.
 *
 * Se usan prioridades distintas porque:
 * - SimulationTask coordina el sistema completo.
 * - ProximityTask debe responder rápido ante interrupciones.
 * - CommandTask puede tener menor prioridad porque solo atiende UI.
 */
#define SHIP_TASK_STACK_SIZE 4096
#define SHIP_TASK_PRIORITY 5

#define SIMULATION_TASK_STACK_SIZE 4096
#define SIMULATION_TASK_PRIORITY 6

#define COMMAND_TASK_STACK_SIZE 4096
#define COMMAND_TASK_PRIORITY 4

#define PROXIMITY_TASK_PRIORITY (SIMULATION_TASK_PRIORITY + 1)

/*
 * Control temporal del movimiento.
 *
 * MOVEMENT_BASE_MS:
 *   Tick lógico de referencia. Se usa para la política de flujo.
 *
 * MOVEMENT_SUBTICK_MS:
 *   Subdivisión del tick base. Permite movimiento fluido.
 *
 * MOVEMENT_SETTLE_MS:
 *   Pequeña pausa para que las ShipTasks se despierten,
 *   muevan y luego se renderice el estado estable.
 */
#define MOVEMENT_BASE_MS 1000
#define MOVEMENT_SUBTICK_MS 100
#define MOVEMENT_SETTLE_MS 20

/*
 * Cantidad de subticks consecutivos sin detección necesarios
 * para considerar que la interrupción terminó.
 *
 * Con MOVEMENT_SUBTICK_MS = 100:
 *   5 subticks = 500 ms sin detección.
 */
#define SENSOR_RELEASE_CONFIRM_SUBTICKS 5


// ======================================================
// VARIABLES GLOBALES PRINCIPALES
// ======================================================

/*
 * Contador usado para confirmar que el sensor dejó de detectar
 * de forma estable antes de levantar las agujas.
 */
static int proximityReleaseCounter = 0;

/*
 * Configuración general del sistema.
 * Se carga con valores por defecto y luego se modifica con CFG_*.
 */
static AppConfig config;

/*
 * Estructuras principales del modelo.
 *
 * demoCanal:
 *   Canal físico de 6 posiciones.
 *
 * leftQueue/rightQueue:
 *   Colas de listos por lado.
 *
 * flowPolicy:
 *   Control de flujo: Letrero, Equidad o Tico.
 */
static Canal demoCanal;
static ReadyQueue leftQueue;
static ReadyQueue rightQueue;
static FlowPolicy flowPolicy;

/*
 * Handles de tareas principales.
 */
static TaskHandle_t simulationTaskHandle = NULL;
static TaskHandle_t commandTaskHandle = NULL;
static TaskHandle_t proximityTaskHandle = NULL;

/*
 * Semáforo para proteger acceso concurrente a las colas.
 *
 * Las colas pueden ser modificadas desde:
 * - SimulationTask
 * - CommandTask
 * - creación de barcos
 * - preempción por scheduler
 * - renderizado
 */
static SemaphoreHandle_t queuesSemaphore = NULL;

/*
 * Banderas globales de estado.
 */
static volatile bool systemRunning = false;
static bool dynamicGenerationEnabled = false;
static bool proximitySafetyActive = false;

/*
 * queueCapacity:
 *   Cantidad máxima interna de barcos por cola.
 *
 * visibleQueueSlots:
 *   Cantidad de barcos visibles por cola en UI/HW.
 *   El enunciado indica máximo 4.
 */
static int queueCapacity = 4;
static int visibleQueueSlots = 4;

/*
 * Buffer temporal para interrupciones.
 *
 * Importante:
 * Los barcos retirados por interrupción NO vuelven a la cola.
 * Se guardan aquí y luego se restauran directamente al canal.
 */
static ShipTask *interruptedShips[PHYSICAL_CANAL_CELLS];
static int interruptedShipCount = 0;


// ======================================================
// PROTOTIPOS DE TAREAS
// ======================================================

static void simulation_task(void *params);
static void command_task(void *params);
static void ship_task_entry(void *params);
static void proximity_task(void *params);


// ======================================================
// PROTOTIPOS DE COMUNICACIÓN Y CREACIÓN
// ======================================================

static void wait_for_ui_start(void);
static bool process_cfg_command(char *line);
static bool process_gen_command(char *line, bool isInitialLoad);
static bool create_ship_from_ui(char sideChar, char typeChar);


// ======================================================
// PROTOTIPOS DE RENDER Y UTILIDADES
// ======================================================

static void render_outputs(void);
static void trim_newline(char *line);


// ======================================================
// PROTOTIPOS DE CONVERSIÓN DE STRINGS A ENUMS
// ======================================================

static SchedulerType scheduler_from_string(const char *text);
static FLOWTYPE flow_from_string(const char *text);
static ShipType ship_type_from_char(char typeChar);
static Direction direction_from_char(char sideChar);


// ======================================================
// PROTOTIPOS DE SCHEDULER Y LOGS
// ======================================================

static int select_scheduler_index(SchedulerType schedulerType, ReadyQueue *queue);

static const char *flow_decision_to_string(FLOWDECISION decision);
static const char *flow_type_to_string(FLOWTYPE type);
static const char *ship_type_to_string(ShipType type);
static const char *direction_to_string(Direction direction);
static const char *state_to_string(ShipState state);


// ======================================================
// PROTOTIPOS DE VELOCIDAD
// ======================================================

static float get_length_speed_factor(void);
static float get_effective_ship_speed(ShipTask *shipTask);


// ======================================================
// PROTOTIPOS DE SINCRONIZACIÓN
// ======================================================

static bool take_queues(void);
static void give_queues(void);


// ======================================================
// PROTOTIPOS DE INTERRUPCIÓN
// ======================================================

static bool is_proximity_safety_active(void);
static void handle_proximity_interrupt(void);
static bool restore_interrupted_ships_to_canal(void);
static void clear_interrupted_ships_buffer(void);


// ======================================================
// PROTOTIPOS DE SIMULACIÓN Y CANAL
// ======================================================

static void notify_ships_and_render(void);
static bool try_release_one_ship_to_canal(int *rrIndexLeft, int *rrIndexRight);
static bool fixed_simulation_finished(void);


// ======================================================
// PROTOTIPOS DE ORDENAMIENTO DE COLAS
// ======================================================

static void reorder_queue_by_scheduler_unlocked(ReadyQueue *queue);
static void reorder_queues_by_scheduler_unlocked(void);


// ======================================================
// PROTOTIPOS DE PREEMPCIÓN POR SCHEDULER
// ======================================================

static bool preempt_ship_to_ready_queue_unlocked(ShipTask *task, const char *reason);
static bool ship_should_not_be_preempted(ShipTask *task);
static void apply_preemptive_scheduler_unlocked(void);
static bool rr_has_waiting_ship_same_origin(ShipTask *task);
static bool scheduler_checkpoint_reentry_allowed(ShipTask *task);


// ======================================================
// app_main
// ======================================================

void app_main(void)
{
    printf("\n\n===============================\n");
    printf("Scheduling Ships - UI Mode\n");
    printf("===============================\n\n");

    /*
     * 1. Cargar configuración base.
     *
     * Estos valores luego se sobreescriben con comandos CFG_*
     * enviados desde la interfaz.
     */
    config_load_defaults(&config);

    /*
     * 2. Inicializar colas.
     *
     * Cada lado del canal tiene su propia cola de listos.
     */
    queue_init(&leftQueue);
    queue_init(&rightQueue);

    /*
     * 3. Crear semáforo para proteger las colas.
     */
    queuesSemaphore = xSemaphoreCreateBinary();

    if (queuesSemaphore == NULL)
    {
        printf("ERROR: No se pudo crear queuesSemaphore\n");
        return;
    }

    /*
     * El semáforo binario se debe liberar una vez después de crearlo,
     * para que la primera tarea pueda tomarlo.
     */
    xSemaphoreGive(queuesSemaphore);

    /*
     * 4. Inicializar hardware.
     *
     * Si el hardware falla, la simulación puede continuar,
     * pero se deshabilita la salida física.
     */
    bool hardwareOk = hardware_init();

    if (!hardwareOk)
    {
        printf("ADVERTENCIA: Hardware no inicializado correctamente. La simulacion continua.\n");
        hardware_set_enabled(false);
    }

    /*
     * 5. Inicializar comunicación serial con la interfaz.
     */
    if (!serial_comm_init())
    {
        printf("ERROR: No se pudo inicializar serial_comm\n");
        return;
    }

    /*
     * 6. Activar bandera de ejecución antes de cargar barcos iniciales.
     *
     * Esto es importante porque wait_for_ui_start() puede recibir GEN
     * iniciales y crear ShipTasks. Si systemRunning estuviera en false,
     * las ShipTasks podrían terminar inmediatamente.
     */
    systemRunning = true;

    /*
     * 7. Esperar configuración desde UI.
     *
     * Esta función bloquea hasta recibir START.
     */
    wait_for_ui_start();

    /*
     * 8. Crear tarea de proximidad.
     *
     * Esta tarea se despierta por notificación del sensor.
     */
    BaseType_t proxResult = xTaskCreate(
        proximity_task,
        "ProximityTask",
        SHIP_TASK_STACK_SIZE,
        NULL,
        PROXIMITY_TASK_PRIORITY,
        &proximityTaskHandle
    );

    if (proxResult != pdPASS)
    {
        printf("ERROR: No se pudo crear ProximityTask\n");
    }
    else
    {
        /*
         * El driver del sensor necesita saber a qué tarea notificar.
         */
        proximity_sensor_set_notify_task(proximityTaskHandle);
    }

    /*
     * 9. Render inicial.
     */
    render_outputs();

    /*
     * 10. Crear tarea principal de simulación.
     */
    BaseType_t simResult = xTaskCreate(
        simulation_task,
        "SimulationTask",
        SIMULATION_TASK_STACK_SIZE,
        NULL,
        SIMULATION_TASK_PRIORITY,
        &simulationTaskHandle
    );

    if (simResult != pdPASS)
    {
        printf("ERROR: No se pudo crear SimulationTask\n");
        systemRunning = false;
        return;
    }

    /*
     * 11. Crear tarea de comandos runtime.
     *
     * Esta tarea atiende:
     * - GEN dinámicos
     * - STOP desde UI
     */
    BaseType_t cmdResult = xTaskCreate(
        command_task,
        "CommandTask",
        COMMAND_TASK_STACK_SIZE,
        NULL,
        COMMAND_TASK_PRIORITY,
        &commandTaskHandle
    );

    if (cmdResult != pdPASS)
    {
        printf("ADVERTENCIA: No se pudo crear CommandTask. No habra generacion dinamica.\n");
    }
}

// ======================================================
// process_cfg_command
// ======================================================

static bool process_cfg_command(char *line)
{
    /*
     * Largo lógico del canal.
     *
     * Nota:
     * El canal físico siempre tiene PHYSICAL_CANAL_CELLS posiciones.
     * El largo lógico afecta la velocidad efectiva de los barcos.
     */
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

    /*
     * Capacidad máxima interna de cada cola.
     */
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

    /*
     * Tipo de scheduler:
     * FCFS, SJF, STRN, Priority, EDF, Round Robin.
     */
    if (strncmp(line, "CFG_S:", 6) == 0)
    {
        config.schedulerType = scheduler_from_string(line + 6);
        printf("CFG scheduler=%s\n", line + 6);
        return true;
    }

    /*
     * Tipo de política de flujo:
     * Letrero, Equidad o Tico.
     */
    if (strncmp(line, "CFG_F:", 6) == 0)
    {
        config.flowType = flow_from_string(line + 6);
        printf("CFG flow=%s\n", line + 6);
        return true;
    }

    /*
     * Parámetro W para política de Equidad.
     */
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

    /*
     * Intervalo del letrero.
     *
     * La UI lo envía en milisegundos.
     * Internamente se convierte a ticks base.
     */
    if (strncmp(line, "CFG_SIGN:", 9) == 0)
    {
        int signIntervalMs = atoi(line + 9);

        config.signInterval =
            (signIntervalMs + MOVEMENT_BASE_MS - 1) / MOVEMENT_BASE_MS;

        if (config.signInterval <= 0)
        {
            config.signInterval = 1;
        }

        printf("CFG signInterval ticks=%d\n", config.signInterval);
        return true;
    }

    /*
     * Quantum para Round Robin.
     *
     * En este diseño representa cantidad de pasos efectivos avanzados.
     */
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

    /*
     * Modo de generación:
     * - Fijo
     * - Dinámico
     */
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

    /*
     * Cantidad visible de barcos por cola.
     *
     * El enunciado indica máximo 4.
     */
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

// ======================================================
// wait_for_ui_start
// ======================================================

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

        /*
         * Handshake con la interfaz.
         */
        if (strcmp(line, "PING") == 0)
        {
            serial_comm_write_line("ACK:PING");
            continue;
        }

        /*
         * Configuración inicial.
         */
        if (strncmp(line, "CFG_", 4) == 0)
        {
            process_cfg_command(line);
            serial_comm_write_line("ACK:CFG");
            continue;
        }

        /*
         * Barcos iniciales.
         */
        if (strncmp(line, "GEN:", 4) == 0)
        {
            char copy[UI_LINE_BUFFER_SIZE];

            strncpy(copy, line, sizeof(copy));
            copy[sizeof(copy) - 1] = '\0';

            process_gen_command(copy, true);
            serial_comm_write_line("ACK:GEN");
            continue;
        }

        /*
         * START:
         * inicializa canal y política de flujo.
         */
        if (strcmp(line, "START") == 0)
        {
            canal_init(&demoCanal, PHYSICAL_CANAL_CELLS, LEFT);

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


// ======================================================
// process_gen_command
// ======================================================

static bool process_gen_command(char *line, bool isInitialLoad)
{
    /*
     * Formato esperado:
     *   GEN:L:N
     *   GEN:R:F
     *   GEN:L:P
     */
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

    /*
     * Si el sistema está en modo fijo, solo se aceptan GEN
     * durante la carga inicial.
     */
    if (!isInitialLoad && !dynamicGenerationEnabled)
    {
        printf("Modo fijo activo. GEN ignorado: %s:%s\n", sideStr, typeStr);
        return false;
    }

    return create_ship_from_ui(sideStr[0], typeStr[0]);
}


// ======================================================
// Cálculo de velocidad efectiva
// ======================================================

static float get_length_speed_factor(void)
{
    if (config.canalLength <= 0)
    {
        return 1.0f;
    }

    /*
     * El canal físico siempre mide PHYSICAL_CANAL_CELLS.
     * El canal lógico afecta la duración del cruce:
     *
     * canalLength = 6  → factor = 1.0
     * canalLength = 12 → factor = 0.5
     * canalLength = 3  → factor = 2.0
     */
    return (float)PHYSICAL_CANAL_CELLS / (float)config.canalLength;
}

static float get_effective_ship_speed(ShipTask *shipTask)
{
    if (shipTask == NULL)
    {
        return 0.0f;
    }

    /*
     * Velocidad efectiva =
     * velocidad base del tipo de barco × factor por largo lógico.
     */
    return (float)getSpeed(&shipTask->ship) * get_length_speed_factor();
}


// ======================================================
// create_ship_from_ui
// ======================================================

static bool create_ship_from_ui(char sideChar, char typeChar)
{
    Direction direction = direction_from_char(sideChar);
    ShipType type = ship_type_from_char(typeChar);

    /*
     * Reservar memoria para la estructura ShipTask.
     *
     * Esta estructura contiene:
     * - el barco
     * - el handle de su tarea
     * - información de movimiento
     * - información de checkpoint
     * - contadores de scheduler
     */
    ShipTask *shipTask = pvPortMalloc(sizeof(ShipTask));

    if (shipTask == NULL)
    {
        /*
         * Este error es importante porque, si no hay memoria,
         * no se puede crear el barco ni su tarea.
         */
        printf("ERROR: No se pudo reservar memoria para ShipTask\n");
        return false;
    }

    /*
     * Limpiar toda la estructura antes de usarla.
     * Esto evita valores basura en banderas como:
     * - hasCheckpoint
     * - preemptedByScheduler
     * - justEntered
     * - rrStepsUsed
     */
    memset(shipTask, 0, sizeof(ShipTask));

    /*
     * Crear el modelo lógico del barco.
     */
    shipTask->ship = createShip(type, direction, PHYSICAL_CANAL_CELLS);

    /*
     * Inicialización de metadatos de la tarea del barco.
     */
    shipTask->handle = NULL;
    shipTask->maxSteps = PHYSICAL_CANAL_CELLS + 5;

    /*
     * Movimiento.
     */
    shipTask->effectiveSpeed = get_effective_ship_speed(shipTask);
    shipTask->moveCredit = 0.0f;

    /*
     * Round Robin.
     *
     * Este contador representa cuántos pasos efectivos
     * ha avanzado el barco durante su quantum actual.
     */
    shipTask->rrStepsUsed = 0;

    /*
     * Nombre de la tarea.
     */
    snprintf(
        shipTask->taskName,
        sizeof(shipTask->taskName),
        "Ship_%d",
        shipTask->ship.id
    );

    /*
     * Crear la tarea FreeRTOS del barco.
     *
     * La tarea no se mueve inmediatamente.
     * Primero queda esperando hasta que el barco cambie a RUNNING.
     */
    BaseType_t result = xTaskCreate(
        ship_task_entry,
        shipTask->taskName,
        SHIP_TASK_STACK_SIZE,
        shipTask,
        SHIP_TASK_PRIORITY,
        &shipTask->handle
    );

    if (result != pdPASS)
    {
        printf("ERROR: No se pudo crear task para barco %d\n", shipTask->ship.id);

        vPortFree(shipTask);
        return false;
    }

    /*
     * Seleccionar cola según origen.
     */
    ReadyQueue *targetQueue =
        direction == LEFT ? &leftQueue : &rightQueue;

    /*
     * Proteger acceso a la cola.
     */
    if (!take_queues())
    {
        printf("ERROR: No se pudo tomar semaforo de colas\n");

        vTaskDelete(shipTask->handle);
        vPortFree(shipTask);

        return false;
    }

    /*
     * Validar capacidad interna de cola.
     */
    if (targetQueue->count >= queueCapacity)
    {
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

    /*
     * Insertar el barco en cola READY.
     */
    if (!queue_add(targetQueue, shipTask))
    {
        printf("ERROR: queue_add fallo para barco %d\n", shipTask->ship.id);

        give_queues();

        vTaskDelete(shipTask->handle);
        vPortFree(shipTask);

        return false;
    }

    give_queues();

    /*
     * Actualizar UI/HW después de crear el barco.
     */
    render_outputs();

    printf(
        "Barco creado desde UI | ID: %d | Lado: %c | Tipo: %c | BaseSpeed: %d | EffectiveSpeed: %.2f\n",
        shipTask->ship.id,
        sideChar,
        typeChar,
        getSpeed(&shipTask->ship),
        shipTask->effectiveSpeed
    );

    return true;
}

// ======================================================
// Estado de interrupción por proximidad
// ======================================================

/*
 * Retorna si el sistema está actualmente en modo de seguridad.
 *
 * Este modo se activa cuando el sensor de proximidad detecta un objeto.
 * Mientras está activo:
 * - No deben entrar nuevos barcos al canal.
 * - El canal se considera bloqueado.
 * - Las agujas se muestran activas.
 * - Los barcos retirados por seguridad se guardan temporalmente.
 */
static bool is_proximity_safety_active(void)
{
    return proximitySafetyActive;
}


// ======================================================
// handle_proximity_interrupt
// ======================================================

/*
 * Maneja el inicio de una interrupción de proximidad.
 *
 * Esta función se ejecuta cuando el sensor detecta un objeto.
 *
 * Responsabilidades:
 * 1. Activar el modo de seguridad.
 * 2. Bloquear el canal.
 * 3. Retirar temporalmente los barcos que estén dentro del canal.
 * 4. Guardar su posición y crédito de movimiento.
 * 5. Almacenar esos barcos en interruptedShips[].
 * 6. Renderizar el estado actualizado en hardware/UI.
 *
 * Importante:
 * Los barcos retirados por interrupción NO vuelven a las colas ready.
 * Esto es diferente a la preempción por scheduler.
 */
static void handle_proximity_interrupt(void)
{
    /*
     * Si ya hay una interrupción activa, no se debe procesar otra.
     *
     * Esto evita:
     * - borrar interruptedShipCount accidentalmente,
     * - perder referencias a barcos retirados,
     * - repetir la evacuación del canal,
     * - provocar parpadeos o estados inconsistentes.
     */
    if (proximitySafetyActive)
    {
        printf("Interrupcion ya activa. Ignorando nuevo trigger.\n");
        return;
    }

    printf(
        "PROXIMITY SENSOR TRIGGERED: entrando en modo de seguridad. "
        "Bajando agujas y evacuando canal.\n"
    );

    /*
     * Activar modo de seguridad.
     *
     * A partir de este punto:
     * - try_release_one_ship_to_canal() no debe admitir barcos.
     * - hardware_render_state() debe reflejar agujas activas.
     * - el canal queda bloqueado.
     */
    proximitySafetyActive = true;

    /*
     * Reiniciar contador de liberación.
     *
     * Este contador se usa luego en SimulationTask para confirmar
     * que el sensor dejó de detectar de forma estable.
     */
    proximityReleaseCounter = 0;

    /*
     * Bloquear el canal.
     *
     * Esto evita nuevas entradas mientras está activa la interrupción.
     */
    canal_block(&demoCanal);

    /*
     * Reiniciar buffer temporal de barcos interrumpidos.
     *
     * Se asume que no debería haber una interrupción previa activa
     * porque se validó proximitySafetyActive al inicio.
     */
    interruptedShipCount = 0;

    /*
     * Si el canal tiene barcos, se deben retirar temporalmente.
     */
    if (!canal_is_empty(&demoCanal))
    {
        /*
         * Primero copiamos los punteros de barcos a un arreglo temporal.
         *
         * Razón:
         * No conviene recorrer y remover directamente sobre demoCanal
         * al mismo tiempo, porque al remover se modifica la estructura
         * que estamos recorriendo.
         */
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

        /*
         * Retirar cada barco encontrado dentro del canal.
         */
        for (int i = 0; i < count; i++)
        {
            ShipTask *task = tasks[i];

            /*
             * Guardar checkpoint del barco.
             *
             * savedPosition:
             *   posición física donde estaba el barco.
             *
             * savedMoveCredit:
             *   crédito acumulado de movimiento.
             *
             * hasCheckpoint:
             *   indica que el barco tiene estado guardado.
             *
             * justEntered:
             *   se limpia para que al restaurar no se trate como
             *   entrada nueva desde cero.
             */
            task->savedPosition = task->ship.position;
            task->savedMoveCredit = task->moveCredit;
            task->hasCheckpoint = true;
            task->justEntered = false;

            /*
             * En una interrupción, el barco deja de estar RUNNING.
             *
             * Ojo:
             * Esto NO significa que vuelva a la cola ready normal.
             * Solo queda temporalmente fuera del canal.
             */
            task->ship.state = READY;

            /*
             * Remover físicamente el barco del canal.
             */
            ShipTask *removedTask = canal_remove_task(&demoCanal, task);

            if (removedTask == NULL)
            {
                printf(
                    "ERROR: no se pudo remover barco %d del canal.\n",
                    task->ship.id
                );
                continue;
            }

            /*
             * Guardar el barco en el buffer de interrupción.
             *
             * Este buffer es exclusivo para recuperación post-interrupción.
             */
            if (interruptedShipCount < PHYSICAL_CANAL_CELLS)
            {
                interruptedShips[interruptedShipCount++] = removedTask;

                printf(
                    "Barco %d retirado por interrupcion | "
                    "savedPosition=%d | savedCredit=%.2f\n",
                    removedTask->ship.id,
                    removedTask->savedPosition,
                    removedTask->savedMoveCredit
                );
            }
            else
            {
                /*
                 * Este caso no debería pasar si el canal tiene máximo
                 * PHYSICAL_CANAL_CELLS barcos.
                 */
                printf("ERROR: buffer de barcos interrumpidos lleno.\n");
            }
        }
    }

    printf(
        "Total barcos interrumpidos guardados: %d\n",
        interruptedShipCount
    );

    /*
     * Renderizar estado inmediatamente:
     * - canal vacío,
     * - agujas activas,
     * - colas sin los barcos interrumpidos,
     * - letrero según comportamiento del hardware.
     */
    render_outputs();

    /*
     * Limpia el latch/evento de interrupción del hardware.
     *
     * En este proyecto se mantiene este clear aquí porque fue el
     * comportamiento que permitió que las agujas bajaran/subieran
     * correctamente con el sensor.
     *
     * La liberación final se confirma en SimulationTask mediante
     * proximityReleaseCounter.
     */
    hardware_clear_interrupt();
}


// ======================================================
// proximity_task
// ======================================================

/*
 * Tarea dedicada a manejar notificaciones del sensor de proximidad.
 *
 * Esta tarea no hace polling continuo.
 * Se queda bloqueada hasta que el driver del sensor la notifica.
 *
 * Ventaja:
 * - evita busy waiting,
 * - responde rápido a eventos de proximidad,
 * - separa la interrupción de la lógica principal de simulación.
 */
static void proximity_task(void *params)
{
    (void)params;

    while (systemRunning)
    {
        /*
         * Espera una notificación desde el sensor.
         *
         * pdTRUE:
         *   limpia el contador de notificaciones al despertar.
         *
         * portMAX_DELAY:
         *   espera indefinidamente hasta que llegue una notificación.
         */
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        if (!systemRunning)
        {
            break;
        }

        /*
         * Si no hay modo de seguridad activo, se procesa la interrupción.
         */
        if (!is_proximity_safety_active())
        {
            handle_proximity_interrupt();
        }
        else
        {
            /*
             * Si ya hay interrupción activa, se ignora este nuevo evento.
             *
             * Importante:
             * No se llama hardware_clear_interrupt() aquí, porque limpiar
             * el hardware en este punto puede causar que SimulationTask crea
             * que el sensor se liberó antes de tiempo.
             */
            printf("ProximityTask: interrupcion ignorada porque ya esta activa.\n");
        }
    }

    vTaskDelete(NULL);
}


// ======================================================
// restore_interrupted_ships_to_canal
// ======================================================

/*
 * Restaura al canal los barcos retirados por una interrupción.
 *
 * Esta función se llama cuando SimulationTask detecta que el sensor
 * dejó de detectar de forma estable.
 *
 * Diferencia importante:
 * - Barcos expulsados por scheduler: vuelven a ready queue.
 * - Barcos retirados por interrupción: vuelven directo al canal.
 *
 * Retorna:
 * - true  si la restauración fue exitosa o no había nada que restaurar.
 * - false si no se pudo restaurar todavía.
 */
static bool restore_interrupted_ships_to_canal(void)
{
    /*
     * Si no hay barcos guardados por interrupción, no hay nada que hacer.
     */
    if (interruptedShipCount <= 0)
    {
        return true;
    }

    printf(
        "Restaurando %d barcos interrumpidos al canal...\n",
        interruptedShipCount
    );

    /*
     * La restauración directa solo se hace si el canal está vacío.
     *
     * Esto evita:
     * - insertar barcos sobre otros barcos,
     * - crear colisiones,
     * - mezclar restauración con movimiento normal.
     */
    if (!canal_is_empty(&demoCanal))
    {
        printf("No se puede restaurar: el canal no esta vacio.\n");
        return false;
    }

    /*
     * Restaurar cada barco en su posición guardada.
     */
    for (int i = 0; i < interruptedShipCount; i++)
    {
        ShipTask *task = interruptedShips[i];

        if (task == NULL)
        {
            continue;
        }

        /*
         * Reinsertar el barco exactamente en la posición donde estaba.
         *
         * canal_enter_at() debe ser estricto:
         * si la posición no está libre, retorna false.
         */
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

        /*
         * Restaurar crédito de movimiento.
         *
         * Esto permite que el barco retome con el progreso temporal
         * que tenía antes de la interrupción.
         */
        task->moveCredit = task->savedMoveCredit;

        /*
         * Limpiar checkpoint de interrupción.
         *
         * Ya no hay estado pendiente por restaurar.
         */
        task->hasCheckpoint = false;
        task->savedPosition = 0;
        task->savedMoveCredit = 0.0f;

        /*
         * No se considera entrada nueva.
         *
         * justEntered = false:
         *   evita que el barco reinicie su movimiento como si acabara
         *   de entrar desde la cola.
         *
         * hasEnteredBefore = true:
         *   indica que el barco ya estaba dentro del canal antes.
         */
        task->justEntered = false;
        task->hasEnteredBefore = true;

        printf(
            "Barco %d restaurado en posicion %d\n",
            task->ship.id,
            task->ship.position
        );
    }

    /*
     * Una vez restaurados todos los barcos, se limpia el buffer temporal.
     */
    clear_interrupted_ships_buffer();

    return true;
}


// ======================================================
// clear_interrupted_ships_buffer
// ======================================================

/*
 * Limpia el arreglo temporal de barcos interrumpidos.
 *
 * Se llama después de restaurar correctamente todos los barcos.
 */
static void clear_interrupted_ships_buffer(void)
{
    for (int i = 0; i < PHYSICAL_CANAL_CELLS; i++)
    {
        interruptedShips[i] = NULL;
    }

    interruptedShipCount = 0;
}

// ======================================================
// simulation_task
// ======================================================

/*
 * Tarea principal de simulación.
 *
 * Esta tarea coordina:
 * - lectura periódica del sensor,
 * - manejo del modo de seguridad,
 * - actualización de la política de flujo,
 * - aplicación de schedulers expulsivos,
 * - admisión de barcos al canal,
 * - notificación de movimiento a las ShipTasks,
 * - renderizado en UI/HW,
 * - finalización automática en modo fijo.
 *
 * No mueve directamente los barcos.
 * En su lugar, despierta las ShipTasks con notify_ships_and_render().
 */
static void simulation_task(void *params)
{
    (void)params;

    /*
     * Índices usados por Round Robin para cada cola.
     *
     * Se mantienen separados porque cada lado tiene su propia cola.
     */
    int rrIndexLeft = 0;
    int rrIndexRight = 0;

    /*
     * Acumulador para saber cuándo se cumple un tick base.
     *
     * El movimiento ocurre cada MOVEMENT_SUBTICK_MS,
     * pero la política de flujo se actualiza cada MOVEMENT_BASE_MS.
     */
    int elapsedBaseMs = 0;

    printf("SimulationTask iniciada.\n\n");

    while (systemRunning)
    {
        // --------------------------------------------------
        // 1. Actualización del sensor de proximidad
        // --------------------------------------------------

        /*
         * Dispara una medición del sensor.
         *
         * El resultado queda disponible para hardware_sensor_active().
         */
        proximity_sensor_trigger_ping();

        /*
         * Si el sensor detecta un objeto y aún no estamos en modo seguridad,
         * se activa la interrupción.
         */
        if (hardware_sensor_active() && !is_proximity_safety_active())
        {
            handle_proximity_interrupt();

            /*
             * IMPORTANTE:
             * handle_proximity_interrupt() limpia el interrupt al final.
             *
             * Si siguiéramos en este mismo subtick, el sistema podría leer
             * hardware_sensor_active() como false y creer que la interrupción
             * ya terminó inmediatamente.
             *
             * Por eso se renderiza, se espera un subtick y se vuelve al inicio.
             */
            render_outputs();
            vTaskDelay(pdMS_TO_TICKS(MOVEMENT_SUBTICK_MS));
            continue;
        }

        // --------------------------------------------------
        // 2. Modo de seguridad activo
        // --------------------------------------------------

        if (is_proximity_safety_active())
        {
            /*
             * Se lee el estado del sensor en este subtick.
             *
             * Si sigue detectando, el contador de liberación vuelve a cero.
             * Si no detecta, el contador aumenta.
             */
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
             * Se limpia el latch del hardware después de usar la lectura.
             *
             * Esto permite que el siguiente proximity_sensor_trigger_ping()
             * actualice el estado real del sensor.
             */
            hardware_clear_interrupt();

            /*
             * Solo se considera que el sensor fue liberado si se acumulan
             * varios subticks consecutivos sin detección.
             *
             * Esto evita parpadeo de agujas por lecturas falsas aisladas.
             */
            if (proximityReleaseCounter >= SENSOR_RELEASE_CONFIRM_SUBTICKS)
            {
                printf("Sensor liberado estable. Intentando restaurar barcos interrumpidos.\n");

                /*
                 * Para restaurar barcos con canal_enter_at(),
                 * primero se debe desbloquear el canal.
                 */
                canal_unblock(&demoCanal);

                if (restore_interrupted_ships_to_canal())
                {
                    printf("Modo de seguridad finalizado. Barcos restaurados. Levantando agujas.\n");

                    /*
                     * Termina el modo seguridad.
                     */
                    proximitySafetyActive = false;
                    proximityReleaseCounter = 0;

                    /*
                     * Mostrar en UI/HW el canal restaurado antes de continuar
                     * con el movimiento normal.
                     */
                    render_outputs();

                    /*
                     * Pausa visual breve para que se note la restauración.
                     */
                    vTaskDelay(pdMS_TO_TICKS(300));
                }
                else
                {
                    /*
                     * Si algo impide restaurar, se mantiene el modo seguridad
                     * y se reintenta en el siguiente subtick.
                     */
                    printf("No se pudo restaurar todavia. Manteniendo modo seguridad.\n");

                    canal_block(&demoCanal);
                    proximityReleaseCounter = 0;
                }
            }

            /*
             * Mientras hay seguridad activa, no se hace admisión ni movimiento.
             */
            render_outputs();
            vTaskDelay(pdMS_TO_TICKS(MOVEMENT_SUBTICK_MS));
            continue;
        }

        // --------------------------------------------------
        // 3. Avance de política de flujo
        // --------------------------------------------------

        /*
         * La política de flujo no se actualiza cada subtick,
         * sino cada MOVEMENT_BASE_MS.
         *
         * Esto evita que Letrero/Equidad/Tico cambien demasiado rápido.
         */
        elapsedBaseMs += MOVEMENT_SUBTICK_MS;

        if (elapsedBaseMs >= MOVEMENT_BASE_MS)
        {
            elapsedBaseMs = 0;

            /*
             * Actualiza el estado interno de la política de flujo.
             * Por ejemplo:
             * - cambio de letrero,
             * - conteo de equidad,
             * - lógica de Tico.
             */
            flow_policy_on_tick(&flowPolicy);

            printf("\n---------- BASE TICK ----------\n");
            printf(
                "FlowPolicy: %s | Direccion actual: %s\n",
                flow_type_to_string(flowPolicy.type),
                flow_decision_to_string(flowPolicy.direction)
            );
        }

        // --------------------------------------------------
        // 4. Preempción antes del movimiento
        // --------------------------------------------------

        /*
         * Antes de admitir/mover, se revisa si STRN o EDF deben expulsar
         * algún barco por la llegada de uno más urgente en la cola.
         *
         * También se reordenan las colas para que UI/HW muestren el mismo
         * orden que usa el scheduler.
         */
        if (take_queues())
        {
            reorder_queues_by_scheduler_unlocked();
            apply_preemptive_scheduler_unlocked();
            give_queues();
        }

        // --------------------------------------------------
        // 5. Intento de admisión antes del movimiento
        // --------------------------------------------------

        /*
         * Se intenta meter un barco antes del movimiento.
         *
         * Esto permite aprovechar espacios libres al inicio del subtick.
         */
        try_release_one_ship_to_canal(&rrIndexLeft, &rrIndexRight);

        // --------------------------------------------------
        // 6. Movimiento de barcos
        // --------------------------------------------------

        /*
         * Despierta las ShipTasks en orden.
         *
         * Cada barco:
         * - acumula crédito,
         * - intenta moverse máximo un paso,
         * - respeta bloqueos por barcos adelante.
         */
        notify_ships_and_render();

        // --------------------------------------------------
        // 7. Preempción después del movimiento
        // --------------------------------------------------

        /*
         * Después del movimiento se vuelve a revisar la preempción.
         *
         * Esto es especialmente importante para Round Robin,
         * porque rrStepsUsed aumenta solo cuando el barco realmente avanzó.
         */
        if (take_queues())
        {
            apply_preemptive_scheduler_unlocked();
            give_queues();
        }

        // --------------------------------------------------
        // 8. Intento de admisión después del movimiento
        // --------------------------------------------------

        /*
         * Se intenta meter otro barco después del movimiento.
         *
         * Esto evita huecos artificiales:
         * si un barco avanzó y liberó la entrada,
         * otro puede entrar en el mismo ciclo de simulación.
         */
        try_release_one_ship_to_canal(&rrIndexLeft, &rrIndexRight);

        // --------------------------------------------------
        // 9. Render final del subtick
        // --------------------------------------------------

        render_outputs();

        // --------------------------------------------------
        // 10. Finalización automática en modo fijo
        // --------------------------------------------------

        if (fixed_simulation_finished())
        {
            printf("\nNo quedan barcos en colas ni en canal. Simulacion fija finalizada.\n");
            break;
        }

        /*
         * Espera hasta el siguiente subtick.
         */
        vTaskDelay(pdMS_TO_TICKS(MOVEMENT_SUBTICK_MS));
    }

    /*
     * Al salir del ciclo, se apaga la bandera global.
     */
    systemRunning = false;

    /*
     * Última notificación/render para dejar el sistema visualmente estable.
     */
    notify_ships_and_render();

    printf("\nSimulationTask finalizada.\n");

    vTaskDelete(NULL);
}


// ======================================================
// command_task
// ======================================================

/*
 * Tarea encargada de recibir comandos de la UI durante ejecución.
 *
 * A diferencia de wait_for_ui_start(), esta tarea corre después de START.
 *
 * Comandos runtime soportados:
 * - GEN:L:N
 * - GEN:R:F
 * - GEN:L:P
 * - STOP
 */
static void command_task(void *params)
{
    (void)params;

    char line[UI_LINE_BUFFER_SIZE];

    printf("CommandTask iniciada. Esperando comandos dinamicos.\n");

    while (systemRunning)
    {
        /*
         * Leer una línea desde la comunicación serial.
         *
         * Si no hay datos, se espera un poco para no consumir CPU
         * innecesariamente.
         */
        int len = serial_comm_read_line(line, sizeof(line));

        if (len <= 0)
        {
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }

        /*
         * Limpiar saltos de línea para facilitar comparaciones.
         */
        trim_newline(line);

        if (strlen(line) == 0)
        {
            continue;
        }

        printf("RX UI runtime: %s\n", line);

        // --------------------------------------------------
        // Comando GEN durante ejecución
        // --------------------------------------------------

        if (strncmp(line, "GEN:", 4) == 0)
        {
            char copy[UI_LINE_BUFFER_SIZE];

            /*
             * Se copia la línea porque process_gen_command() usa strtok(),
             * que modifica el contenido original.
             */
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

            /*
             * Actualizar estado después de intentar generar.
             */
            render_outputs();
        }

        // --------------------------------------------------
        // Comando STOP
        // --------------------------------------------------

        else if (strcmp(line, "STOP") == 0)
        {
            printf("STOP recibido desde UI. Reiniciando sistema...\n");

            /*
             * La UI espera este ACK antes de volver al setup.
             */
            serial_comm_write_line("ACK:STOP");

            /*
             * Apagar bandera de ejecución.
             */
            systemRunning = false;

            /*
             * Espera corta para permitir que el ACK salga por serial.
             */
            vTaskDelay(pdMS_TO_TICKS(300));

            /*
             * Reinicio suave de la ESP.
             *
             * Esto simplifica mucho la limpieza:
             * - colas,
             * - tareas,
             * - canal,
             * - semáforos,
             * - estado de interrupción,
             * - flow policy.
             */
            esp_restart();
        }

        // --------------------------------------------------
        // Comando no reconocido
        // --------------------------------------------------

        else
        {
            printf("Comando runtime ignorado: %s\n", line);
        }
    }

    printf("CommandTask finalizada.\n");

    vTaskDelete(NULL);
}


// ======================================================
// ship_task_entry
// ======================================================

/*
 * Tarea individual de cada barco.
 *
 * Cada barco creado desde la UI tiene su propia ShipTask.
 *
 * Esta tarea:
 * - espera hasta que el barco esté RUNNING,
 * - acumula crédito de movimiento,
 * - intenta avanzar máximo un paso por subtick,
 * - respeta bloqueos dentro del canal,
 * - actualiza rrStepsUsed si el scheduler es Round Robin,
 * - termina cuando el barco llega a FINISHED.
 */
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
        shipTask->effectiveSpeed
    );

    /*
     * Ciclo de vida de la tarea del barco.
     *
     * La tarea puede pasar varias veces por READY → RUNNING
     * si el barco es expulsado por RR/STRN/EDF.
     */
    while (systemRunning)
    {
        // --------------------------------------------------
        // 1. Esperar a que el barco sea admitido al canal
        // --------------------------------------------------

        /*
         * Mientras el barco no esté RUNNING, se queda bloqueado.
         *
         * Esto evita busy waiting.
         *
         * La tarea se despierta cuando:
         * - el barco entra al canal,
         * - el barco es expulsado y debe salir del ciclo RUNNING,
         * - el sistema se detiene.
         */
        while (systemRunning && shipTask->ship.state != RUNNING)
        {
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        }

        if (!systemRunning)
        {
            break;
        }

        /*
         * Si despierta con checkpoint activo, significa que viene de una
         * expulsión/restauración. No se limpia aquí.
         *
         * El checkpoint se limpia en:
         * - try_release_one_ship_to_canal(), para preempción por scheduler,
         * - restore_interrupted_ships_to_canal(), para interrupciones.
         */
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

        /*
         * Si el barco nunca había entrado antes, el primer movimiento
         * aprovecha la misma notificación de entrada.
         */
        bool firstMovementAfterEntry = !shipTask->hasEnteredBefore;
        shipTask->hasEnteredBefore = true;

        // --------------------------------------------------
        // 2. Ciclo de movimiento mientras el barco está activo
        // --------------------------------------------------

        while (systemRunning && shipTask->ship.state != FINISHED)
        {
            /*
             * Normalmente la tarea espera una notificación por subtick.
             *
             * En el primer movimiento después de entrar, no espera de nuevo
             * porque ya fue despertada por la admisión al canal.
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

            /*
             * Si el barco fue expulsado por scheduler o retirado del canal,
             * su estado vuelve a READY. En ese caso sale del ciclo de
             * movimiento y vuelve a esperar admisión.
             */
            if (shipTask->ship.state == READY)
            {
                break;
            }

            /*
             * Si por alguna razón no está RUNNING ni READY ni FINISHED,
             * se ignora este subtick.
             */
            if (shipTask->ship.state != RUNNING)
            {
                continue;
            }

            // --------------------------------------------------
            // 3. Primer subtick tras entrada normal
            // --------------------------------------------------

            if (shipTask->justEntered)
            {
                /*
                 * Cuando un barco acaba de entrar al canal, no se le da
                 * inmediatamente un paso completo.
                 *
                 * Se reinicia el crédito para que empiece a acumular de forma
                 * proporcional a los subticks.
                 */
                shipTask->justEntered = false;
                shipTask->moveCredit = 0.0f;

                printf(
                    "[%s] Primer subtick tras entrada | Posicion: %d\n",
                    shipTask->taskName,
                    shipTask->ship.position
                );
            }

            // --------------------------------------------------
            // 4. Acumulación de crédito de movimiento
            // --------------------------------------------------

            /*
             * En cada subtick se suma solo una fracción de la velocidad.
             *
             * Ejemplo con MOVEMENT_SUBTICK_MS = 100:
             *
             * Normal effectiveSpeed = 1.0:
             *   suma 0.1 por subtick.
             *   avanza cada 10 subticks ≈ 1 segundo.
             *
             * Pesquero effectiveSpeed = 2.0:
             *   suma 0.2 por subtick.
             *   avanza cada 5 subticks ≈ 0.5 segundos.
             *
             * Patrulla effectiveSpeed = 3.0:
             *   suma 0.3 por subtick.
             *   avanza aprox. cada 3-4 subticks.
             */
            float creditIncrement =
                shipTask->effectiveSpeed *
                ((float)MOVEMENT_SUBTICK_MS / 1000.0f);

            shipTask->moveCredit += creditIncrement;

            bool movedAtLeastOnce = false;
            bool blocked = false;

            // --------------------------------------------------
            // 5. Intento de movimiento unitario
            // --------------------------------------------------

            /*
             * Aunque el barco tenga velocidad alta, solo intenta un paso
             * por subtick.
             *
             * Esto evita saltos visuales.
             */
            if (
                shipTask->moveCredit >= 1.0f &&
                shipTask->ship.state != FINISHED &&
                systemRunning
            )
            {
                bool moved = canal_move_one_step(&demoCanal, shipTask);

                if (moved)
                {
                    movedAtLeastOnce = true;
                    shipTask->moveCredit -= 1.0f;

                    /*
                     * En Round Robin, el quantum se mide en pasos efectivos,
                     * no en tiempo bloqueado.
                     *
                     * Por eso rrStepsUsed aumenta solo si el barco realmente
                     * avanzó una posición.
                     */
                    if (config.schedulerType == SCHD_RR)
                    {
                        shipTask->rrStepsUsed++;
                    }
                }
                else
                {
                    /*
                     * Si el barco no pudo avanzar porque la siguiente posición
                     * estaba ocupada, se considera bloqueado en este subtick.
                     *
                     * Se limpia el crédito para evitar que acumule muchos pasos
                     * mientras está pegado detrás de otro barco.
                     */
                    blocked = true;
                    shipTask->moveCredit = 0.0f;
                }
            }

            /*
             * Este bloque se deja vacío intencionalmente.
             *
             * Antes se usaba para logs de acumulación de crédito, pero esos
             * logs saturan el serial y hacen difícil leer la simulación.
             */
            if (!movedAtLeastOnce && !blocked && shipTask->moveCredit < 1.0f)
            {
                /*
                 * Debug opcional:
                 *
                 * printf(
                 *     "[%s] Acumulando credito | Credito: %.2f | Incremento: %.2f | Velocidad efectiva: %.2f\n",
                 *     shipTask->taskName,
                 *     shipTask->moveCredit,
                 *     creditIncrement,
                 *     shipTask->effectiveSpeed
                 * );
                 */
            }
        }

        /*
         * Si el barco terminó, sale del ciclo principal de la task.
         */
        if (shipTask->ship.state == FINISHED)
        {
            break;
        }
    }

    // --------------------------------------------------
    // 6. Finalización de la ShipTask
    // --------------------------------------------------

    if (shipTask->ship.state == FINISHED)
    {
        printf(
            "[%s] Finalizo cruce | Estado: %s\n",
            shipTask->taskName,
            state_to_string(shipTask->ship.state)
        );

        /*
         * Liberar memoria reservada para esta ShipTask.
         *
         * Importante:
         * El barco ya no debe estar en ninguna cola ni en el canal
         * cuando llega a este punto.
         */
        vPortFree(shipTask);
    }

    vTaskDelete(NULL);
}

// ======================================================
// render_outputs
// ======================================================

/*
 * Actualiza las salidas del sistema:
 *
 * 1. Hardware:
 *    - LEDs de colas.
 *    - LEDs del canal.
 *    - letrero/sentido.
 *    - agujas/interrupción.
 *
 * 2. Interfaz gráfica:
 *    - estado de colas.
 *    - estado del canal.
 *    - estado del letrero.
 *    - estado de agujas.
 *
 * Antes de renderizar, las colas se reordenan según el scheduler.
 * Esto permite que la UI/HW muestren el mismo orden que se usará para admitir barcos.
 *
 * Importante:
 * Esta función toma queuesSemaphore.
 * Por eso NO debe llamarse desde una sección que ya tenga tomado ese semáforo.
 */
static void render_outputs(void)
{
    if (take_queues())
    {
        /*
         * Reordenar colas antes de mostrarlas.
         *
         * Esto aplica para schedulers como:
         * - SJF
         * - STRN
         * - Priority
         * - EDF
         *
         * No aplica para:
         * - FCFS
         * - RR
         */
        reorder_queues_by_scheduler_unlocked();

        /*
         * Render físico en hardware.
         */
        hardware_render_state(
            &leftQueue,
            &rightQueue,
            &demoCanal,
            &flowPolicy
        );

        /*
         * Envío de estado a la interfaz.
         */
        ui_bridge_send_state(
            &leftQueue,
            &rightQueue,
            &demoCanal,
            &flowPolicy
        );

        give_queues();
    }
}


// ======================================================
// trim_newline
// ======================================================

/*
 * Elimina caracteres de salto de línea al final de una cadena.
 *
 * Se usa principalmente para limpiar comandos recibidos por serial.
 *
 * Ejemplo:
 *   "STOP\r\n" → "STOP"
 */
static void trim_newline(char *line)
{
    if (line == NULL)
    {
        return;
    }

    size_t len = strlen(line);

    while (
        len > 0 &&
        (line[len - 1] == '\n' || line[len - 1] == '\r')
    )
    {
        line[len - 1] = '\0';
        len--;
    }
}


// ======================================================
// notify_ships_and_render
// ======================================================

/*
 * Notifica a los barcos dentro del canal para que intenten moverse.
 *
 * La función canal_notify_ships_ordered() debe despertar los barcos en orden
 * seguro, típicamente primero el que está más adelante en el sentido de avance.
 *
 * Esto es importante porque evita huecos artificiales:
 *
 * Ejemplo:
 *   Si hay dos barcos consecutivos:
 *
 *      [A][B]
 *
 *   primero debe moverse A, luego B.
 *   Si se moviera B primero, podría quedar bloqueado innecesariamente.
 */
static void notify_ships_and_render(void)
{
    /*
     * Notificar barcos en orden de movimiento.
     */
    canal_notify_ships_ordered(&demoCanal);

    /*
     * Pequeña espera para permitir que las ShipTasks despierten,
     * procesen su movimiento y actualicen su estado.
     */
    vTaskDelay(pdMS_TO_TICKS(MOVEMENT_SETTLE_MS));

    /*
     * Renderizar el estado posterior al movimiento.
     */
    render_outputs();
}


// ======================================================
// try_release_one_ship_to_canal
// ======================================================

/*
 * Intenta admitir un barco desde una de las colas hacia el canal.
 *
 * Esta función integra dos decisiones:
 *
 * 1. Política de flujo:
 *    Decide desde qué lado se permite entrar:
 *    - FLOW_LEFT
 *    - FLOW_RIGHT
 *    - FLOW_NONE
 *
 * 2. Scheduler:
 *    Decide cuál barco de esa cola entra:
 *    - FCFS
 *    - SJF
 *    - STRN
 *    - Priority
 *    - EDF
 *    - Round Robin
 *
 * También maneja reingreso por checkpoint cuando el barco fue expulsado
 * por un scheduler preemptivo.
 *
 * Retorna:
 *   true  si algún barco entró al canal.
 *   false si no entró nadie.
 */
static bool try_release_one_ship_to_canal(int *rrIndexLeft, int *rrIndexRight)
{
    /*
     * Si hay interrupción activa, no se permite admitir barcos.
     */
    if (is_proximity_safety_active())
    {
        return false;
    }

    /*
     * Proteger acceso a colas.
     */
    if (!take_queues())
    {
        return false;
    }

    /*
     * Reordenar las colas según el scheduler actual.
     *
     * Esto asegura que la selección sea coherente con lo que se muestra
     * en UI/HW.
     */
    reorder_queues_by_scheduler_unlocked();

    /*
     * La política de flujo decide desde qué lado se puede liberar un barco.
     */
    FLOWDECISION decision = flow_policy_select_side(
        &flowPolicy,
        &leftQueue,
        &rightQueue
    );

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

    /*
     * Si la política de flujo no permite ningún lado,
     * o si la cola seleccionada está vacía, no entra nadie.
     */
    if (selectedQueue == NULL || queue_is_empty(selectedQueue))
    {
        give_queues();
        return false;
    }

    /*
     * Selección del barco dentro de la cola.
     */
    int selectedIndex = -1;

    if (config.schedulerType == SCHD_RR)
    {
        /*
         * Round Robin necesita un índice rotativo independiente para cada lado.
         */
        if (releasedSide == FLOW_LEFT)
        {
            selectedIndex = scheduler_rr(
                selectedQueue,
                config.rrQuantum,
                rrIndexLeft
            );
        }
        else
        {
            selectedIndex = scheduler_rr(
                selectedQueue,
                config.rrQuantum,
                rrIndexRight
            );
        }
    }
    else
    {
        /*
         * El resto de schedulers devuelven directamente el índice seleccionado.
         */
        selectedIndex = select_scheduler_index(
            config.schedulerType,
            selectedQueue
        );
    }

    if (selectedIndex < 0)
    {
        give_queues();
        return false;
    }

    ShipTask *selectedTask = queue_get(selectedQueue, selectedIndex);

    if (selectedTask == NULL)
    {
        give_queues();
        return false;
    }

    /*
     * Determinar si este barco está reingresando por checkpoint.
     *
     * Esto ocurre cuando fue expulsado por un scheduler preemptivo:
     * - RR
     * - STRN
     * - EDF
     *
     * No se usa para interrupciones, porque los barcos interrumpidos
     * se restauran directamente desde interruptedShips[].
     */
    bool reenteringCheckpoint =
        selectedTask->hasCheckpoint &&
        selectedTask->preemptedByScheduler;

    bool entered = false;

    if (reenteringCheckpoint)
    {
        /*
         * Para STRN/EDF, un barco expulsado no puede reingresar
         * si todavía hay otro barco más urgente corriendo.
         *
         * Esto evita el error donde un barco largo expulsado vuelve
         * inmediatamente y bloquea al barco corto/urgente.
         */
        if (!scheduler_checkpoint_reentry_allowed(selectedTask))
        {
            give_queues();
            return false;
        }

        /*
         * Reingreso exacto a la posición guardada.
         *
         * canal_enter_at() debe ser estricto:
         * si la posición está ocupada, retorna false.
         */
        entered = canal_enter_at(
            &demoCanal,
            selectedTask,
            selectedTask->savedPosition
        );
    }
    else
    {
        /*
         * Entrada normal desde el extremo del canal.
         */
        entered = canal_enter(&demoCanal, selectedTask);
    }

    if (entered)
    {
        printf(
            "Barco %d entro exitosamente desde %s\n",
            selectedTask->ship.id,
            flow_decision_to_string(releasedSide)
        );

        if (reenteringCheckpoint)
        {
            /*
             * Restaurar crédito de movimiento acumulado antes de la expulsión.
             */
            selectedTask->moveCredit = selectedTask->savedMoveCredit;

            /*
             * Limpiar estado de checkpoint por scheduler.
             */
            selectedTask->hasCheckpoint = false;
            selectedTask->preemptedByScheduler = false;
            selectedTask->savedPosition = 0;
            selectedTask->savedMoveCredit = 0.0f;

            /*
             * No se considera entrada nueva desde cero.
             */
            selectedTask->justEntered = false;
            selectedTask->hasEnteredBefore = true;

            /*
             * Reiniciar contadores de scheduler.
             */
            selectedTask->schedulerRunTimeMs = 0;
            selectedTask->rrStepsUsed = 0;

            printf(
                "Barco %d reingreso por checkpoint en posicion %d\n",
                selectedTask->ship.id,
                selectedTask->ship.position
            );
        }
        else
        {
            /*
             * Entrada normal:
             * el primer subtick se usa para estabilizar entrada,
             * no para avanzar inmediatamente.
             */
            selectedTask->justEntered = true;

            /*
             * Reiniciar contadores de scheduler.
             */
            selectedTask->schedulerRunTimeMs = 0;
            selectedTask->rrStepsUsed = 0;
        }

        /*
         * Una vez que el barco entra al canal, se elimina de la cola.
         */
        queue_remove(selectedQueue, selectedIndex);

        /*
         * Notificar a la política de flujo que un barco fue liberado.
         *
         * Esto es útil para:
         * - Equidad: contar cuántos barcos han pasado por lado.
         * - Letrero/Tico: actualizar estado interno si aplica.
         */
        flow_policy_on_ship_released(&flowPolicy, releasedSide);
    }

    give_queues();

    return entered;
}


// ======================================================
// fixed_simulation_finished
// ======================================================

/*
 * Determina si la simulación fija ya terminó.
 *
 * La simulación fija termina cuando:
 * - NO está habilitada la generación dinámica,
 * - la cola izquierda está vacía,
 * - la cola derecha está vacía,
 * - el canal está vacío.
 *
 * En modo dinámico no se usa esta condición para terminar automáticamente.
 */
static bool fixed_simulation_finished(void)
{
    bool finished = false;

    if (!take_queues())
    {
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


// ======================================================
// reorder_queue_by_scheduler_unlocked
// ======================================================

/*
 * Reordena una cola según el scheduler actual.
 *
 * Sufijo _unlocked:
 *   indica que esta función asume que quien la llama ya tomó
 *   queuesSemaphore.
 *
 * No reordena:
 * - FCFS:
 *   porque el orden natural de la cola ya es el orden de llegada.
 *
 * - Round Robin:
 *   porque depende de su lógica rotativa, no de ordenar por prioridad.
 *
 * Sí reordena:
 * - SJF
 * - STRN
 * - Priority
 * - EDF
 *
 * Objetivo:
 *   Que la UI/HW muestren la cola en el mismo orden en que el scheduler
 *   la va a atender.
 */
static void reorder_queue_by_scheduler_unlocked(ReadyQueue *queue)
{
    if (queue == NULL)
    {
        return;
    }

    /*
     * FCFS y RR conservan el orden propio de la cola.
     */
    if (config.schedulerType == SCHED_FCFS ||
        config.schedulerType == SCHD_RR)
    {
        return;
    }

    if (queue->count <= 1)
    {
        return;
    }

    /*
     * workQueue:
     *   copia temporal de la cola original.
     *
     * sortedQueue:
     *   cola reconstruida en orden de scheduler.
     */
    ReadyQueue workQueue;
    ReadyQueue sortedQueue;

    queue_init(&workQueue);
    queue_init(&sortedQueue);

    /*
     * Copiar punteros de tareas.
     *
     * No se duplican barcos, solo se reorganizan referencias.
     */
    for (int i = 0; i < queue->count; i++)
    {
        workQueue.tasks[i] = queue->tasks[i];
    }

    workQueue.count = queue->count;

    /*
     * Construir sortedQueue seleccionando repetidamente el mejor candidato.
     */
    while (!queue_is_empty(&workQueue))
    {
        int selectedIndex = select_scheduler_index(
            config.schedulerType,
            &workQueue
        );

        if (selectedIndex < 0)
        {
            break;
        }

        ShipTask *selectedTask = queue_get(&workQueue, selectedIndex);

        if (selectedTask == NULL)
        {
            break;
        }

        queue_add(&sortedQueue, selectedTask);
        queue_remove(&workQueue, selectedIndex);
    }

    /*
     * Copiar el orden resultante de vuelta a la cola original.
     */
    queue->count = sortedQueue.count;

    for (int i = 0; i < sortedQueue.count; i++)
    {
        queue->tasks[i] = sortedQueue.tasks[i];
    }
}


// ======================================================
// reorder_queues_by_scheduler_unlocked
// ======================================================

/*
 * Reordena ambas colas de listos.
 *
 * Sufijo _unlocked:
 *   esta función también asume que queuesSemaphore ya fue tomado.
 */
static void reorder_queues_by_scheduler_unlocked(void)
{
    reorder_queue_by_scheduler_unlocked(&leftQueue);
    reorder_queue_by_scheduler_unlocked(&rightQueue);
}

// ======================================================
// preempt_ship_to_ready_queue_unlocked
// ======================================================

/*
 * Expulsa un barco del canal por decisión del scheduler.
 *
 * Esta función se usa para calendarizadores expulsivos:
 * - Round Robin
 * - STRN / SRTN
 * - EDF
 *
 * Diferencia importante:
 *
 * Interrupción:
 *   canal → interruptedShips[] → canal
 *
 * Scheduler expulsivo:
 *   canal → readyQueue → scheduler → canal
 *
 * Sufijo _unlocked:
 *   esta función asume que quien la llama ya tomó queuesSemaphore.
 */
static bool preempt_ship_to_ready_queue_unlocked(ShipTask *task, const char *reason)
{
    if (task == NULL)
    {
        return false;
    }

    /*
     * Seleccionar la cola a la que debe volver el barco.
     *
     * Un barco expulsado vuelve a la cola de su mismo origen.
     */
    ReadyQueue *targetQueue =
        task->ship.origin == LEFT ? &leftQueue : &rightQueue;

    if (targetQueue == NULL)
    {
        return false;
    }

    /*
     * Validar capacidad ANTES de remover del canal.
     *
     * Esto es más seguro que remover primero y luego descubrir que
     * no hay espacio en la cola, porque en ese caso el barco podría
     * quedar fuera del canal y fuera de la cola.
     */
    if (targetQueue->count >= queueCapacity)
    {
        printf(
            "ERROR: cola %s llena. No se puede expulsar barco %d por %s\n",
            direction_to_string(task->ship.origin),
            task->ship.id,
            reason
        );

        return false;
    }

    printf(
        "PREEMPT [%s] Barco %d removido del canal | pos=%d | credit=%.2f | remaining=%d\n",
        reason,
        task->ship.id,
        task->ship.position,
        task->moveCredit,
        task->ship.remainingTime
    );

    /*
     * Guardar checkpoint.
     *
     * savedPosition:
     *   posición actual dentro del canal.
     *
     * savedMoveCredit:
     *   crédito de movimiento acumulado.
     *
     * hasCheckpoint:
     *   indica que debe reingresar por posición guardada.
     *
     * preemptedByScheduler:
     *   permite distinguir esta expulsión de una interrupción.
     */
    task->savedPosition = task->ship.position;
    task->savedMoveCredit = task->moveCredit;
    task->hasCheckpoint = true;
    task->preemptedByScheduler = true;

    /*
     * Reiniciar contadores relacionados con scheduler.
     *
     * En RR, rrStepsUsed mide pasos efectivos dentro del quantum actual.
     * Al expulsar, se reinicia para el próximo turno.
     */
    task->schedulerRunTimeMs = 0;
    task->rrStepsUsed = 0;

    /*
     * El barco deja de estar RUNNING.
     *
     * Al volver a READY, su ShipTask saldrá del ciclo interno de movimiento
     * y quedará esperando una nueva admisión.
     */
    task->ship.state = READY;

    /*
     * Remover físicamente del canal.
     */
    ShipTask *removedTask = canal_remove_task(&demoCanal, task);

    if (removedTask == NULL)
    {
        printf("ERROR: no se pudo expulsar barco %d del canal\n", task->ship.id);
        return false;
    }

    /*
     * Reinsertar en la cola de listos.
     *
     * A partir de aquí, el scheduler decidirá cuándo vuelve a entrar.
     */
    if (!queue_add(targetQueue, removedTask))
    {
        printf("ERROR: no se pudo reencolar barco expulsado %d\n", removedTask->ship.id);
        return false;
    }

    /*
     * Despertar la ShipTask.
     *
     * Esto permite que la tarea salga de su ciclo RUNNING y vuelva a esperar
     * hasta que el scheduler la admita de nuevo.
     */
    if (removedTask->handle != NULL)
    {
        xTaskNotifyGive(removedTask->handle);
    }

    return true;
}


// ======================================================
// ship_should_not_be_preempted
// ======================================================

/*
 * Determina si un barco NO debería ser expulsado.
 *
 * Esta función evita expulsiones innecesarias o peligrosas.
 *
 * Casos donde no conviene expulsar:
 * - task NULL.
 * - barco no está RUNNING.
 * - remainingTime <= 0.
 * - barco ya está en la última posición antes de salir.
 */
static bool ship_should_not_be_preempted(ShipTask *task)
{
    if (task == NULL)
    {
        return true;
    }

    if (task->ship.state != RUNNING)
    {
        return true;
    }

    /*
     * Si ya no le queda tiempo lógico, no tiene sentido expulsarlo.
     */
    if (task->ship.remainingTime <= 0)
    {
        return true;
    }

    /*
     * Si viene de LEFT, sale por la última posición del canal.
     */
    if (
        task->ship.origin == LEFT &&
        task->ship.position >= demoCanal.length - 1
    )
    {
        return true;
    }

    /*
     * Si viene de RIGHT, sale por la posición 0.
     */
    if (
        task->ship.origin == RIGHT &&
        task->ship.position <= 0
    )
    {
        return true;
    }

    return false;
}


// ======================================================
// apply_preemptive_scheduler_unlocked
// ======================================================

/*
 * Aplica la lógica de calendarizadores expulsivos.
 *
 * Schedulers expulsivos implementados:
 * - Round Robin:
 *     expulsa por quantum de pasos efectivos.
 *
 * - STRN / SRTN:
 *     expulsa si en la cola aparece un barco con menor remainingTime.
 *
 * - EDF:
 *     expulsa si en la cola aparece un barco con deadline más urgente.
 *
 * Sufijo _unlocked:
 *   esta función asume que queuesSemaphore ya fue tomado.
 */
static void apply_preemptive_scheduler_unlocked(void)
{
    /*
     * Si el scheduler actual no es expulsivo, no hay nada que hacer.
     */
    if (
        config.schedulerType != SCHD_RR &&
        config.schedulerType != SCHED_STRN &&
        config.schedulerType != SCHED_EDF
    )
    {
        return;
    }

    /*
     * Si el canal está vacío, no hay barco corriendo para expulsar.
     */
    if (canal_is_empty(&demoCanal))
    {
        return;
    }

    // --------------------------------------------------
    // Caso 1: Round Robin
    // --------------------------------------------------

    if (config.schedulerType == SCHD_RR)
    {
        ShipTask *tasks[PHYSICAL_CANAL_CELLS];
        int count = 0;

        /*
         * Copiar primero los barcos en canal a un arreglo temporal.
         *
         * Esto evita modificar demoCanal mientras se recorre.
         */
        for (int i = 0; i < demoCanal.length; i++)
        {
            ShipTask *task = demoCanal.ships_inside.tasks[i];

            if (task != NULL && count < PHYSICAL_CANAL_CELLS)
            {
                tasks[count++] = task;
            }
        }

        /*
         * Revisar cada barco activo para ver si consumió su quantum.
         */
        for (int i = 0; i < count; i++)
        {
            ShipTask *task = tasks[i];

            if (task == NULL || task->ship.state != RUNNING)
            {
                continue;
            }

            if (ship_should_not_be_preempted(task))
            {
                continue;
            }

            /*
             * En este proyecto, el quantum de RR se interpreta como
             * cantidad de pasos efectivos avanzados, no como tiempo bloqueado.
             *
             * rrStepsUsed aumenta solamente cuando canal_move_one_step()
             * devuelve true.
             */
            if (task->rrStepsUsed >= config.rrQuantum)
            {
                /*
                 * Si no hay otro barco esperando en la misma cola,
                 * no tiene sentido expulsar este barco solo para que
                 * vuelva a entrar inmediatamente.
                 */
                if (!rr_has_waiting_ship_same_origin(task))
                {
                    continue;
                }

                preempt_ship_to_ready_queue_unlocked(task, "RR quantum");
            }
        }

        return;
    }

    // --------------------------------------------------
    // Caso 2: STRN / EDF
    // --------------------------------------------------

    /*
     * En STRN y EDF se compara:
     *
     * - mejor candidato en cola
     * contra
     * - barcos que ya están RUNNING en el canal.
     *
     * Solo se comparan barcos del mismo origen.
     *
     * Razón:
     * El control de sentido lo maneja FlowPolicy y el canal no debe mezclar
     * sentidos contrarios por seguridad.
     */
    ReadyQueue *candidateQueues[2] = { &leftQueue, &rightQueue };

    for (int q = 0; q < 2; q++)
    {
        ReadyQueue *queue = candidateQueues[q];

        if (queue == NULL || queue_is_empty(queue))
        {
            continue;
        }

        /*
         * Ordenar la cola para que el mejor candidato quede al frente.
         */
        reorder_queue_by_scheduler_unlocked(queue);

        ShipTask *candidate = queue_get(queue, 0);

        if (candidate == NULL)
        {
            continue;
        }

        /*
         * worstRunning representa el barco RUNNING que debería ser expulsado
         * si el candidato de cola tiene mayor prioridad según el scheduler.
         */
        ShipTask *worstRunning = NULL;

        for (int i = 0; i < demoCanal.length; i++)
        {
            ShipTask *running = demoCanal.ships_inside.tasks[i];

            if (running == NULL || running->ship.state != RUNNING)
            {
                continue;
            }

            if (ship_should_not_be_preempted(running))
            {
                continue;
            }

            /*
             * No preemptar barcos de sentido contrario.
             */
            if (running->ship.origin != candidate->ship.origin)
            {
                continue;
            }

            /*
             * STRN:
             * Si el candidato en cola tiene menor remainingTime que uno
             * corriendo, el que corre puede ser expulsado.
             */
            if (config.schedulerType == SCHED_STRN)
            {
                if (candidate->ship.remainingTime < running->ship.remainingTime)
                {
                    if (
                        worstRunning == NULL ||
                        running->ship.remainingTime > worstRunning->ship.remainingTime
                    )
                    {
                        worstRunning = running;
                    }
                }
            }

            /*
             * EDF:
             * Menor deadline significa mayor urgencia.
             */
            if (config.schedulerType == SCHED_EDF)
            {
                if (candidate->ship.deadline < running->ship.deadline)
                {
                    if (
                        worstRunning == NULL ||
                        running->ship.deadline > worstRunning->ship.deadline
                    )
                    {
                        worstRunning = running;
                    }
                }
            }
        }

        /*
         * Si se encontró un barco corriendo menos conveniente,
         * se expulsa y vuelve a la cola de listos.
         */
        if (worstRunning != NULL)
        {
            if (config.schedulerType == SCHED_STRN)
            {
                preempt_ship_to_ready_queue_unlocked(
                    worstRunning,
                    "SRTN menor remaining time"
                );
            }
            else if (config.schedulerType == SCHED_EDF)
            {
                preempt_ship_to_ready_queue_unlocked(
                    worstRunning,
                    "EDF deadline mas urgente"
                );
            }
        }
    }
}


// ======================================================
// rr_has_waiting_ship_same_origin
// ======================================================

/*
 * Verifica si hay otro barco esperando en la cola del mismo origen.
 *
 * Se usa en RR para evitar expulsar un barco si no hay nadie más
 * esperando turno.
 */
static bool rr_has_waiting_ship_same_origin(ShipTask *task)
{
    if (task == NULL)
    {
        return false;
    }

    ReadyQueue *queue =
        task->ship.origin == LEFT ? &leftQueue : &rightQueue;

    if (queue == NULL)
    {
        return false;
    }

    return !queue_is_empty(queue);
}


// ======================================================
// scheduler_checkpoint_reentry_allowed
// ======================================================

/*
 * Decide si un barco expulsado por scheduler puede reingresar al canal.
 *
 * Problema que evita:
 * En STRN/EDF, un barco largo o menos urgente puede ser expulsado,
 * pero luego podría reingresar inmediatamente por checkpoint y quedar
 * delante del barco más urgente, bloqueándolo.
 *
 * Solución:
 * - RR puede reingresar normalmente cuando le toque.
 * - STRN espera si hay otro barco RUNNING con menor remainingTime.
 * - EDF espera si hay otro barco RUNNING con deadline más urgente.
 */
static bool scheduler_checkpoint_reentry_allowed(ShipTask *task)
{
    if (task == NULL)
    {
        return false;
    }

    /*
     * Si no viene de expulsión por scheduler, no se aplica restricción.
     */
    if (!task->hasCheckpoint || !task->preemptedByScheduler)
    {
        return true;
    }

    /*
     * Round Robin puede reingresar cuando el scheduler lo seleccione.
     */
    if (config.schedulerType == SCHD_RR)
    {
        return true;
    }

    /*
     * Para otros schedulers no expulsivos, permitir por defecto.
     */
    if (
        config.schedulerType != SCHED_STRN &&
        config.schedulerType != SCHED_EDF
    )
    {
        return true;
    }

    /*
     * Revisar si existe un barco más urgente corriendo en el canal.
     */
    for (int i = 0; i < demoCanal.length; i++)
    {
        ShipTask *running = demoCanal.ships_inside.tasks[i];

        if (running == NULL || running->ship.state != RUNNING)
        {
            continue;
        }

        if (running->ship.origin != task->ship.origin)
        {
            continue;
        }

        if (config.schedulerType == SCHED_STRN)
        {
            if (running->ship.remainingTime < task->ship.remainingTime)
            {
                printf(
                    "STRN: Barco %d espera checkpoint porque Barco %d tiene menor remainingTime (%d < %d)\n",
                    task->ship.id,
                    running->ship.id,
                    running->ship.remainingTime,
                    task->ship.remainingTime
                );

                return false;
            }
        }

        if (config.schedulerType == SCHED_EDF)
        {
            if (running->ship.deadline < task->ship.deadline)
            {
                printf(
                    "EDF: Barco %d espera checkpoint porque Barco %d tiene deadline mas urgente (%d < %d)\n",
                    task->ship.id,
                    running->ship.id,
                    running->ship.deadline,
                    task->ship.deadline
                );

                return false;
            }
        }
    }

    return true;
}


// ======================================================
// Conversiones desde texto / caracteres
// ======================================================

/*
 * Convierte el texto recibido desde la UI al enum interno SchedulerType.
 */
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

    /*
     * Valor seguro por defecto.
     */
    return SCHED_FCFS;
}


/*
 * Convierte el texto recibido desde la UI al enum interno FLOWTYPE.
 */
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

    /*
     * Valor seguro por defecto.
     */
    return FLOW_SIGN;
}


/*
 * Convierte el carácter del comando GEN al tipo de barco.
 *
 * N → NORMAL
 * F → FISHING
 * P → PATROL
 */
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


/*
 * Convierte el carácter del comando GEN al origen del barco.
 *
 * L → LEFT
 * R → RIGHT
 */
static Direction direction_from_char(char sideChar)
{
    if (sideChar == 'R' || sideChar == 'r')
    {
        return RIGHT;
    }

    return LEFT;
}


// ======================================================
// Selección de scheduler
// ======================================================

/*
 * Selecciona el índice del barco que debe entrar según el scheduler.
 *
 * Round Robin se maneja aparte en try_release_one_ship_to_canal(),
 * porque necesita rrIndexLeft y rrIndexRight.
 */
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
         * Fallback: RR real se maneja en try_release_one_ship_to_canal().
         */
        return scheduler_fcfs(queue);

    default:
        return -1;
    }
}


// ======================================================
// Funciones auxiliares para impresión de logs
// ======================================================

/*
 * Convierte una decisión de flujo a texto.
 */
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


/*
 * Convierte tipo de política de flujo a texto.
 */
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


/*
 * Convierte tipo de barco a texto.
 */
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


/*
 * Convierte dirección a texto.
 */
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


/*
 * Convierte estado de barco a texto.
 */
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


// ======================================================
// Semáforo de colas
// ======================================================

/*
 * Toma el semáforo que protege leftQueue y rightQueue.
 *
 * Se usa para evitar que varias tareas modifiquen o lean las colas
 * al mismo tiempo.
 */
static bool take_queues(void)
{
    if (queuesSemaphore == NULL)
    {
        return false;
    }

    return xSemaphoreTake(queuesSemaphore, portMAX_DELAY) == pdTRUE;
}


/*
 * Libera el semáforo de colas.
 */
static void give_queues(void)
{
    if (queuesSemaphore == NULL)
    {
        return;
    }

    xSemaphoreGive(queuesSemaphore);
}
