#include "serial.h"
#include <windows.h>
#include <setupapi.h>
#include <initguid.h>
#include <devguid.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

HANDLE hSerial = INVALID_HANDLE_VALUE;

/* --------------------------------------------------
 * Detect ESP32 COM port (CH9102: VID_1A86 PID_55D4)
 * -------------------------------------------------- */
bool FindESP32ComPort(char *outPath, size_t outSize) {
    HDEVINFO hDevInfo;
    SP_DEVINFO_DATA devInfo;
    DWORD i = 0;

    hDevInfo = SetupDiGetClassDevs(
        &GUID_DEVCLASS_PORTS,
        NULL, NULL,
        DIGCF_PRESENT
    );

    if (hDevInfo == INVALID_HANDLE_VALUE)
        return false;

    devInfo.cbSize = sizeof(devInfo);

    while (SetupDiEnumDeviceInfo(hDevInfo, i++, &devInfo)) {
        char instanceId[256];
        char friendlyName[256];

        if (!SetupDiGetDeviceInstanceIdA(
                hDevInfo,
                &devInfo,
                instanceId,
                sizeof(instanceId),
                NULL))
            continue;

        if (!strstr(instanceId, "VID_1A86&PID_55D4"))
            continue;

        if (!SetupDiGetDeviceRegistryPropertyA(
                hDevInfo,
                &devInfo,
                SPDRP_FRIENDLYNAME,
                NULL,
                (PBYTE)friendlyName,
                sizeof(friendlyName),
                NULL))
            continue;

        char *p = strstr(friendlyName, "(COM");
        if (!p) continue;

        char *end = strchr(p, ')');
        if (!end) continue;

        snprintf(outPath, outSize, "\\\\.\\%.*s",
                 (int)(end - p - 1), p + 1);

        SetupDiDestroyDeviceInfoList(hDevInfo);
        return true;
    }

    SetupDiDestroyDeviceInfoList(hDevInfo);
    return false;
}

/* --------------------------------------------------
 * Serial init
 * -------------------------------------------------- */
void SerialInit(void) {
    char comPath[32];

    if (!FindESP32ComPort(comPath, sizeof(comPath))) {
        OutputDebugStringA("ESP32 not found\n");
        return;
    }

    hSerial = CreateFileA(
        comPath,
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        0,
        NULL
    );

    if (hSerial == INVALID_HANDLE_VALUE) {
        OutputDebugStringA("Failed to open ESP32 COM port\n");
        return;
    }

    DCB dcb = {0};
    dcb.DCBlength = sizeof(dcb);
    GetCommState(hSerial, &dcb);

    dcb.BaudRate = CBR_9600;
    dcb.ByteSize = 8;
    dcb.StopBits = ONESTOPBIT;
    dcb.Parity   = NOPARITY;

    SetCommState(hSerial, &dcb);

    COMMTIMEOUTS timeouts = {0};
    timeouts.ReadIntervalTimeout = 50;
    timeouts.ReadTotalTimeoutConstant = 50;
    timeouts.WriteTotalTimeoutConstant = 50;

    SetCommTimeouts(hSerial, &timeouts);
}

/* --------------------------------------------------
 * Write
 * -------------------------------------------------- */
void SerialWrite(const char *data, size_t size) {
    if (hSerial == INVALID_HANDLE_VALUE)
        return;

    DWORD written = 0;
    if (!WriteFile(hSerial, data, size, &written, NULL))
        return;

    return;
}

/* --------------------------------------------------
 * Read (buffer-safe)
 * -------------------------------------------------- */
void SerialRead(char *buffer) {

}

/* --------------------------------------------------
 * Close
 * -------------------------------------------------- */
void SerialClose(void) {
    if (hSerial != INVALID_HANDLE_VALUE) {
        CloseHandle(hSerial);
        hSerial = INVALID_HANDLE_VALUE;
    }
}
