#include "serial_comm.h"

#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/usb_serial_jtag.h"
#include "esp_err.h"

#define SERIAL_RX_BUFFER_SIZE 512
#define SERIAL_TX_BUFFER_SIZE 512

static bool serialInitialized = false;

bool serial_comm_init(void)
{
    if (usb_serial_jtag_is_driver_installed()) {
        serialInitialized = true;
        return true;
    }

    usb_serial_jtag_driver_config_t config = {
        .tx_buffer_size = SERIAL_TX_BUFFER_SIZE,
        .rx_buffer_size = SERIAL_RX_BUFFER_SIZE
    };

    esp_err_t err = usb_serial_jtag_driver_install(&config);

    if (err != ESP_OK) {
        printf("ERROR: usb_serial_jtag_driver_install fallo: %s\n", esp_err_to_name(err));
        serialInitialized = false;
        return false;
    }

    serialInitialized = true;

    return true;
}

int serial_comm_read_line(char *buffer, size_t bufferSize)
{
    if (!serialInitialized || buffer == NULL || bufferSize == 0) {
        return -1;
    }

    size_t index = 0;

    while (index < bufferSize - 1) {
        uint8_t byte = 0;

        int readBytes = usb_serial_jtag_read_bytes(
            &byte,
            1,
            pdMS_TO_TICKS(50)
        );

        if (readBytes <= 0) {
            if (index == 0) {
                return 0;
            }

            continue;
        }

        if (byte == '\r') {
            continue;
        }

        if (byte == '\n') {
            break;
        }

        buffer[index] = (char) byte;
        index++;
    }

    buffer[index] = '\0';

    return (int) index;
}

bool serial_comm_write_text(const char *text)
{
    if (!serialInitialized || text == NULL) {
        return false;
    }

    size_t len = strlen(text);

    if (len == 0) {
        return true;
    }

    int written = usb_serial_jtag_write_bytes(
        text,
        len,
        pdMS_TO_TICKS(100)
    );

    return written == (int) len;
}

bool serial_comm_write_line(const char *line)
{
    if (line == NULL) {
        return false;
    }

    if (!serial_comm_write_text(line)) {
        return false;
    }

    return serial_comm_write_text("\n");
}