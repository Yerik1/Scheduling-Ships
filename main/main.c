#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "ship_factory.h"
#include "ship_task.h"
#include "canal.h"

#define DEMO_CANAL_LENGTH 10
#define DEMO_SHIP_COUNT 3

static Canal demoCanal;
static ShipTask demoShips[DEMO_SHIP_COUNT];

static void demo_ship_task(void *params);
static void init_demo_ship_task(ShipTask *shipTask, ShipType type, Direction direction, int canalLength);

static const char *ship_type_to_string(ShipType type);
static const char *direction_to_string(Direction direction);
static const char *state_to_string(ShipState state);

void app_main(void)
{
    printf("\n\n===============================\n");
    printf("DEMO: Tasks reales + Canal + Semaforos\n");
    printf("===============================\n\n");

    canal_init(&demoCanal, DEMO_CANAL_LENGTH, LEFT);

    printf("Canal inicializado con largo %d\n", DEMO_CANAL_LENGTH);

    /*
     * Todos salen desde LEFT para permitir que entren varios al canal
     * en el mismo sentido.
     *
     * NORMAL avanza lento.
     * FISHING avanza mas rapido.
     * PATROL avanza aun mas rapido.
     *
     * Esto prueba que un barco rapido no pueda saltar encima de otro.
     */

    init_demo_ship_task(&demoShips[0], NORMAL, LEFT, DEMO_CANAL_LENGTH);
    init_demo_ship_task(&demoShips[1], FISHING, LEFT, DEMO_CANAL_LENGTH);
    init_demo_ship_task(&demoShips[2], PATROL, LEFT, DEMO_CANAL_LENGTH);

    for (int i = 0; i < DEMO_SHIP_COUNT; i++) {
        printf(
            "Creando task para barco %d | Tipo: %s | Origen: %s | Velocidad: %d\n",
            demoShips[i].ship.id,
            ship_type_to_string(demoShips[i].ship.type),
            direction_to_string(demoShips[i].ship.origin),
            demoShips[i].ship.speed
        );

        BaseType_t result = xTaskCreate(
            demo_ship_task,
            demoShips[i].taskName,
            4096,
            &demoShips[i],
            5,
            &demoShips[i].handle
        );

        if (result != pdPASS) {
            printf("ERROR: No se pudo crear task para barco %d\n", demoShips[i].ship.id);
        }

        /*
         * Este delay pequeño evita que todos intenten entrar exactamente
         * en el mismo instante, pero siguen corriendo concurrentemente.
         */
        vTaskDelay(pdMS_TO_TICKS(300));
    }
}

static void init_demo_ship_task(ShipTask *shipTask, ShipType type, Direction direction, int canalLength)
{
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

static void demo_ship_task(void *params)
{
    ShipTask *shipTask = (ShipTask *) params;

    if (shipTask == NULL) {
        vTaskDelete(NULL);
        return;
    }

    printf(
        "[%s] Iniciada | ID: %d | Tipo: %s | Origen: %s | Estado: %s\n",
        shipTask->taskName,
        shipTask->ship.id,
        ship_type_to_string(shipTask->ship.type),
        direction_to_string(shipTask->ship.origin),
        state_to_string(shipTask->ship.state)
    );

    bool entered = false;

    while (!entered) {
        entered = canal_enter(&demoCanal, shipTask);

        if (!entered) {
            printf(
                "[%s] No pudo entrar todavia. Esperando...\n",
                shipTask->taskName
            );

            vTaskDelay(pdMS_TO_TICKS(500));
        }
    }

    printf(
        "[%s] Entro al canal | Posicion inicial: %d\n",
        shipTask->taskName,
        shipTask->ship.position
    );

    while (shipTask->ship.state != FINISHED) {
        bool moved = canal_move_task(&demoCanal, shipTask);

        if (!moved) {
            printf(
                "[%s] No pudo moverse. Posicion actual: %d. Reintentando...\n",
                shipTask->taskName,
                shipTask->ship.position
            );
        }

        /*
         * Delay por tick lógico de movimiento.
         * Todos los barcos tienen su propia task y despiertan periódicamente.
         */
        vTaskDelay(pdMS_TO_TICKS(700));
    }

    printf(
        "[%s] Finalizo cruce | Estado: %s\n",
        shipTask->taskName,
        state_to_string(shipTask->ship.state)
    );

    vTaskDelete(NULL);
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