//
// Created by USER on 28/01/2026.
//

#include "serial.h"
#include <windows.h>
#include <stdio.h>

HANDLE hSerial;
void SerialInit(void) {

    hSerial = CreateFile("\\\\.\\COM4", GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);

    if (hSerial == INVALID_HANDLE_VALUE) {
        return;
    }

    DCB dcbSerialParams = {0};
    dcbSerialParams.DCBlength = sizeof(dcbSerialParams);
    GetCommState(hSerial, &dcbSerialParams);
    dcbSerialParams.BaudRate = CBR_9600;
    dcbSerialParams.ByteSize = 8;
    dcbSerialParams.StopBits = ONESTOPBIT;
    dcbSerialParams.Parity   = NOPARITY;
    SetCommState(hSerial, &dcbSerialParams);

    COMMTIMEOUTS timeouts = {0};
    timeouts.ReadIntervalTimeout = 50;
    timeouts.ReadTotalTimeoutConstant = 50;
    SetCommTimeouts(hSerial, &timeouts);
}
void SerialWrite(const char* data, size_t size) {
    DWORD bytesWritten;
    WriteFile(hSerial, data, size, &bytesWritten, NULL);
}
void SerialRead(char* data) {
    DWORD bytesRead;
    if (ReadFile(hSerial, data, sizeof(data)-1, &bytesRead, NULL)) {
        printf("Received: %s\n", data);
    }
}

void SerialClose(void) {
    CloseHandle(hSerial);
}
