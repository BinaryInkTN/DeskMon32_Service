//
// Created by USER on 28/01/2026.
//

#ifndef SERVICE_SERIAL_H
#define SERVICE_SERIAL_H
#include <stdint.h>
void SerialInit(void);
void SerialWrite(const char* data, size_t size);
void SerialRead(char* data);
void SerialClose(void);
#endif //SERVICE_SERIAL_H