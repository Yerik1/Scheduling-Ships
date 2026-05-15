//
// Created by user on 5/5/2026.
//

#include "hw_controller.h"

#include <stdio.h>

static bool hardwareInitialized = false;
static bool hardwareEnabled = true;
static int hwVisibleQueueSlots = 4;

bool hardware_init(void)
{
    bool ok = true;

    printf("Inicializando hardware...\n");

    if (!led_strips_init())
    {
        printf("ERROR: No se pudieron inicializar las tiras LED\n");
        ok = false;
    }

    if (!direction_leds_init())
    {
        printf("ERROR: No se pudieron inicializar los LEDs de direccion\n");
        ok = false;
    }

    if (!proximity_sensor_init())
    {
        printf("ERROR: No se pudo inicializar el sensor de proximidad\n");
        ok = false;
    }

    hardwareInitialized = ok;

    if (hardwareInitialized)
    {
        printf("Hardware inicializado correctamente\n");
    }
    else
    {
        printf("Hardware inicializado con errores\n");
    }

    return hardwareInitialized;
}

void hardware_render_state(
    ReadyQueue *leftQueue,
    ReadyQueue *rightQueue,
    Canal *canal,
    FlowPolicy *flowPolicy)
{
    if (!hardwareEnabled)
    {
        return;
    }

    if (!hardwareInitialized)
    {
        return;
    }

    if (leftQueue == NULL || rightQueue == NULL || canal == NULL || flowPolicy == NULL)
    {
        return;
    }

    /*
     * Representa las colas laterales.
     */
    led_strips_render_left_queue(leftQueue, hwVisibleQueueSlots);
    led_strips_render_right_queue(rightQueue, hwVisibleQueueSlots);

    /*
     * Representa las posiciones ocupadas dentro del canal.
     */
    led_strips_render_canal(canal);

    /*
     * Representa la direccion actual del flujo/letrero.
     */
    if (canal != NULL && canal->isBlocked) {
        direction_leds_clear();
    } else {
        direction_leds_set(flowPolicy->direction);
    }
}

bool hardware_interrupt_triggered(void)
{
    if (!hardwareEnabled)
    {
        return false;
    }

    if (!hardwareInitialized)
    {
        return false;
    }

    return proximity_sensor_was_triggered();
}

bool hardware_sensor_active(void)
{
    if (!hardwareEnabled)
    {
        return false;
    }

    if (!hardwareInitialized)
    {
        return false;
    }

    return proximity_sensor_is_active();
}

void hardware_clear_interrupt(void)
{
    if (!hardwareEnabled)
    {
        return;
    }

    if (!hardwareInitialized)
    {
        return;
    }

    proximity_sensor_clear_trigger();
}

void hardware_set_enabled(bool enabled)
{
    hardwareEnabled = enabled;
}

bool hardware_init_canal_only(void)
{
    bool ok = led_strips_init_canal_only();

    hardwareInitialized = ok;

    if (ok)
    {
        printf("Solo tira LED del canal inicializada correctamente\n");
    }

    return ok;
}

void hardware_render_canal_only(Canal *canal)
{
    if (!hardwareEnabled || !hardwareInitialized || canal == NULL)
    {
        return;
    }

    led_strips_render_canal(canal);
}

void hardware_set_visible_queue_slots(int visibleSlots)
{
    if (visibleSlots < 1)
    {
        visibleSlots = 1;
    }

    if (visibleSlots > 4)
    {
        visibleSlots = 4;
    }

    hwVisibleQueueSlots = visibleSlots;
}