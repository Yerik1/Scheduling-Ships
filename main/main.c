#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "ship_factory.h"
#include "ship_task.h"
#include "ready_queue.h"
#include "Scheduler.h"
#include "canal.h"

#define DEMO_CANAL_LENGTH 10

typedef enum {
    DEMO_SCHED_FCFS,
    DEMO_SCHED_SJF,
    DEMO_SCHED_STRN,
    DEMO_SCHED_PRIORITY,
    DEMO_SCHED_EDF
} DemoSchedulerType;

static ShipTask make_demo_ship_task(ShipType type, Direction direction, int canalLength);
static void print_ship_info(const char *prefix, ShipTask *task);
static const char *ship_type_to_string(ShipType type);
static const char *direction_to_string(Direction direction);
static const char *state_to_string(ShipState state);

static void run_task_through_canal(Canal *canal, ShipTask *task);
static int select_scheduler(DemoSchedulerType schedulerType, ReadyQueue *queue);

static void demo_1_single_queue_fcfs(void);
static void demo_2_scheduler_selection(void);
static void demo_3_two_queues_simple_flow(void);

void app_main(void)
{
    printf("\n\n===============================\n");
    printf("Scheduling Ships - Demo Serial\n");
    printf("===============================\n\n");

    demo_1_single_queue_fcfs();

    vTaskDelay(pdMS_TO_TICKS(2000));

    demo_2_scheduler_selection();

    vTaskDelay(pdMS_TO_TICKS(2000));

    demo_3_two_queues_simple_flow();

    printf("\nTodas las demos finalizaron.\n");
}

static ShipTask make_demo_ship_task(ShipType type, Direction direction, int canalLength)
{
    ShipTask task;

    memset(&task, 0, sizeof(ShipTask));

    task.ship = createShip(type, direction, canalLength);
    task.handle = NULL;
    task.maxSteps = canalLength + 2;

    snprintf(
        task.taskName,
        sizeof(task.taskName),
        "Ship_%d",
        task.ship.id
    );

    return task;
}

static void demo_1_single_queue_fcfs(void)
{
    printf("\n===============================\n");
    printf("DEMO 1: Una cola izquierda + FCFS + Canal\n");
    printf("===============================\n");

    Canal canal;
    ReadyQueue leftQueue;

    canal_init(&canal, DEMO_CANAL_LENGTH, LEFT);
    queue_init(&leftQueue);

    static ShipTask tasks[3];

    tasks[0] = make_demo_ship_task(NORMAL, LEFT, DEMO_CANAL_LENGTH);
    tasks[1] = make_demo_ship_task(FISHING, LEFT, DEMO_CANAL_LENGTH);
    tasks[2] = make_demo_ship_task(PATROL, LEFT, DEMO_CANAL_LENGTH);

    for (int i = 0; i < 3; i++) {
        queue_add(&leftQueue, &tasks[i]);
        print_ship_info("Agregado a cola izquierda", &tasks[i]);
    }

    while (!queue_is_empty(&leftQueue)) {
        int selectedIndex = scheduler_fcfs(&leftQueue);

        if (selectedIndex < 0) {
            printf("No se pudo seleccionar barco con FCFS.\n");
            break;
        }

        ShipTask *selectedTask = queue_get(&leftQueue, selectedIndex);

        printf("\nFCFS selecciono:\n");
        print_ship_info("Seleccionado", selectedTask);

        queue_remove(&leftQueue, selectedIndex);

        run_task_through_canal(&canal, selectedTask);

        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    printf("\nDEMO 1 finalizada.\n");
}

static void demo_2_scheduler_selection(void)
{
    printf("\n===============================\n");
    printf("DEMO 2: Comparacion de calendarizadores\n");
    printf("===============================\n");

    DemoSchedulerType schedulers[] = {
        DEMO_SCHED_FCFS,
        DEMO_SCHED_SJF,
        DEMO_SCHED_STRN,
        DEMO_SCHED_PRIORITY,
        DEMO_SCHED_EDF
    };

    const char *schedulerNames[] = {
        "FCFS",
        "SJF",
        "STRN",
        "PRIORITY",
        "EDF"
    };

    for (int s = 0; s < 5; s++) {
        printf("\n--- Scheduler: %s ---\n", schedulerNames[s]);

        ReadyQueue queue;
        queue_init(&queue);

        static ShipTask tasks[5][3];

        tasks[s][0] = make_demo_ship_task(NORMAL, LEFT, DEMO_CANAL_LENGTH);
        tasks[s][1] = make_demo_ship_task(FISHING, LEFT, DEMO_CANAL_LENGTH);
        tasks[s][2] = make_demo_ship_task(PATROL, LEFT, DEMO_CANAL_LENGTH);

        queue_add(&queue, &tasks[s][0]);
        queue_add(&queue, &tasks[s][1]);
        queue_add(&queue, &tasks[s][2]);

        printf("Barcos disponibles:\n");
        for (int i = 0; i < queue.count; i++) {
            print_ship_info("En cola", queue_get(&queue, i));
        }

        printf("\nOrden de seleccion:\n");

        while (!queue_is_empty(&queue)) {
            int selectedIndex = select_scheduler(schedulers[s], &queue);

            if (selectedIndex < 0) {
                printf("No se pudo seleccionar mas barcos.\n");
                break;
            }

            ShipTask *selectedTask = queue_get(&queue, selectedIndex);

            print_ship_info("Seleccionado", selectedTask);

            queue_remove(&queue, selectedIndex);
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    printf("\nDEMO 2 finalizada.\n");
}

static void demo_3_two_queues_simple_flow(void)
{
    printf("\n===============================\n");
    printf("DEMO 3: Dos colas + flujo simple\n");
    printf("===============================\n");

    printf("Politica temporal:\n");
    printf("- Si hay barcos a la izquierda, pasan primero.\n");
    printf("- Si izquierda esta vacia, pasan los de la derecha.\n");
    printf("- Scheduler usado: FCFS.\n\n");

    Canal canal;
    ReadyQueue leftQueue;
    ReadyQueue rightQueue;

    canal_init(&canal, DEMO_CANAL_LENGTH, LEFT);
    queue_init(&leftQueue);
    queue_init(&rightQueue);

    static ShipTask leftTasks[2];
    static ShipTask rightTasks[2];

    leftTasks[0] = make_demo_ship_task(NORMAL, LEFT, DEMO_CANAL_LENGTH);
    leftTasks[1] = make_demo_ship_task(PATROL, LEFT, DEMO_CANAL_LENGTH);

    rightTasks[0] = make_demo_ship_task(FISHING, RIGHT, DEMO_CANAL_LENGTH);
    rightTasks[1] = make_demo_ship_task(NORMAL, RIGHT, DEMO_CANAL_LENGTH);

    queue_add(&leftQueue, &leftTasks[0]);
    queue_add(&leftQueue, &leftTasks[1]);

    queue_add(&rightQueue, &rightTasks[0]);
    queue_add(&rightQueue, &rightTasks[1]);

    printf("Cola izquierda:\n");
    for (int i = 0; i < leftQueue.count; i++) {
        print_ship_info("Izquierda", queue_get(&leftQueue, i));
    }

    printf("\nCola derecha:\n");
    for (int i = 0; i < rightQueue.count; i++) {
        print_ship_info("Derecha", queue_get(&rightQueue, i));
    }

    while (!queue_is_empty(&leftQueue) || !queue_is_empty(&rightQueue)) {
        ReadyQueue *selectedQueue = NULL;
        const char *queueName = NULL;

        if (!queue_is_empty(&leftQueue)) {
            selectedQueue = &leftQueue;
            queueName = "izquierda";
        } else if (!queue_is_empty(&rightQueue)) {
            selectedQueue = &rightQueue;
            queueName = "derecha";
        }

        if (selectedQueue == NULL) {
            break;
        }

        int selectedIndex = scheduler_fcfs(selectedQueue);

        if (selectedIndex < 0) {
            printf("No se pudo seleccionar barco de la cola %s.\n", queueName);
            break;
        }

        ShipTask *selectedTask = queue_get(selectedQueue, selectedIndex);

        printf("\nFCFS selecciono barco de cola %s:\n", queueName);
        print_ship_info("Seleccionado", selectedTask);

        queue_remove(selectedQueue, selectedIndex);

        run_task_through_canal(&canal, selectedTask);

        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    printf("\nDEMO 3 finalizada.\n");
}

static void run_task_through_canal(Canal *canal, ShipTask *task)
{
    if (canal == NULL || task == NULL) {
        return;
    }

    printf("\nIntentando entrar al canal...\n");

    if (!canal_enter(canal, task)) {
        printf("ERROR: El barco %d no pudo entrar al canal.\n", task->ship.id);
        return;
    }

    int safetyCounter = 0;
    int maxIterations = DEMO_CANAL_LENGTH + 5;

    while (task->ship.state != FINISHED && safetyCounter < maxIterations) {
        vTaskDelay(pdMS_TO_TICKS(500));

        if (!canal_move_task(canal, task)) {
            printf(
                "Barco %d no pudo moverse en este tick. Posicion actual: %d\n",
                task->ship.id,
                task->ship.position
            );
        }

        safetyCounter++;
    }

    if (task->ship.state == FINISHED) {
        printf("Barco %d completo el cruce.\n", task->ship.id);
    } else {
        printf("ADVERTENCIA: Barco %d no finalizo dentro del limite de seguridad.\n", task->ship.id);
    }
}

static int select_scheduler(DemoSchedulerType schedulerType, ReadyQueue *queue)
{
    switch (schedulerType) {
        case DEMO_SCHED_FCFS:
            return scheduler_fcfs(queue);

        case DEMO_SCHED_SJF:
            return scheduler_sjf(queue);

        case DEMO_SCHED_STRN:
            return scheduler_strn(queue);

        case DEMO_SCHED_PRIORITY:
            return scheduler_priority(queue);

        case DEMO_SCHED_EDF:
            return scheduler_edf(queue);

        default:
            return -1;
    }
}

static void print_ship_info(const char *prefix, ShipTask *task)
{
    if (task == NULL) {
        printf("%s | NULL\n", prefix);
        return;
    }

    printf(
        "%s | ID: %d | Tipo: %s | Origen: %s | Destino: %s | Estado: %s | "
        "Prioridad: %d | Burst: %d | Remaining: %d | Deadline: %d | Pos: %d\n",
        prefix,
        task->ship.id,
        ship_type_to_string(task->ship.type),
        direction_to_string(task->ship.origin),
        direction_to_string(task->ship.destination),
        state_to_string(task->ship.state),
        task->ship.priority,
        task->ship.burstTime,
        task->ship.remainingTime,
        task->ship.deadline,
        task->ship.position
    );
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