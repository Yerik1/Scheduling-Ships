#ifndef SERIAL_COMM_H
#define SERIAL_COMM_H

#include <stdbool.h>
#include <stddef.h>

bool serial_comm_init(void);

int serial_comm_read_line(char *buffer, size_t bufferSize);

bool serial_comm_write_text(const char *text);

bool serial_comm_write_line(const char *line);

#endif // SERIAL_COMM_H