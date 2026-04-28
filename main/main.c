#include <stdio.h>

#include "ship_factory.h"
#include "ship_task.h"

void app_main(void) {
    printf("Scheduling Ships iniciado\n");

    int canalLength = 10;

    struct Ship normalShip = createShip(NORMAL, LEFT, canalLength);
    struct Ship fishingShip = createShip(FISHING, RIGHT, canalLength);
    struct Ship patrolShip = createShip(PATROL, LEFT, canalLength);

    createShipTask(normalShip);
    createShipTask(fishingShip);
    createShipTask(patrolShip);
}