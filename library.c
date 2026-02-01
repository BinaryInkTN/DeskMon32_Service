#include "library.h"
#include <tchar.h>
#include <strsafe.h>
#include <aclapi.h>
#include <stdio.h>
#include <windows.h>
#include "serial.h"

#include "metrics.h"

#define SVC_NAME TEXT("DeskMon32_Service")

static LibContext ctx = {};

#pragma pack(push, 1)
typedef struct {
    uint8_t header[2];

    float cpu;
    float ram;

    float cpu_temp;
    float cpu_fan;

    float gpu_fan;
    float gpu_util;
    float gpu_temp;

    char  drive_letters[MAX_DRIVES];
    float drive_used[MAX_DRIVES];
    float drive_total[MAX_DRIVES];
    float drive_free[MAX_DRIVES];

    uint8_t drive_count;
    uint32_t timestamp;
} MetricPacket;
#pragma pack(pop)

VOID WINAPI SvcMain(DWORD dwArgc, LPTSTR *lpszArgv);
VOID WINAPI SvcCtrlHandler(DWORD dwCtrl);
VOID ReportSvcStatus(DWORD dwCurrentState, DWORD dwWin32ExitCode, DWORD dwWaitHint);
VOID SvcReportEvent(LPTSTR szFunction);
VOID SvcInit(DWORD dwArgc, LPTSTR *lpszArgv);
DWORD WINAPI ServiceWorkerThread(LPVOID lpParam)
{
    DeviceMetrics metrics;
    ZeroMemory(&metrics, sizeof(metrics));

    SerialInit();

    PollDeviceMetrics(&metrics);
    Sleep(1000);

    const uint32_t start_time = GetTickCount();

    while (WaitForSingleObject(ctx.ghSvcStopEvent, 1000) == WAIT_TIMEOUT)
    {
        PollDeviceMetrics(&metrics);

        MetricPacket packet;
        ZeroMemory(&packet, sizeof(packet));

        packet.header[0] = 0xAA;
        packet.header[1] = 0x55;

        packet.cpu      = (float)metrics.current_cpu_usage;
        packet.ram      = (float)metrics.current_ram_usage;

        packet.cpu_temp = metrics.current_cpu_temperature;
        packet.cpu_fan  = metrics.current_cpu_fan_speed;

        packet.gpu_fan  = metrics.current_gpu_fan_speed;
        packet.gpu_util = metrics.current_gpu_utilisation;
        packet.gpu_temp = metrics.current_gpu_temperature;

        packet.timestamp = GetTickCount() - start_time;

        packet.drive_count = metrics.drive_count;
        if (packet.drive_count > MAX_DRIVES)
            packet.drive_count = MAX_DRIVES;

        for (int i = 0; i < packet.drive_count; ++i)
        {
            packet.drive_letters[i] = metrics.drive_letters[i];
            packet.drive_used[i]    = (float)metrics.drive_used_percent[i];
            packet.drive_total[i]   = (float)metrics.drive_total_gb[i];
            packet.drive_free[i]    = (float)metrics.drive_free_gb[i];
        }

        SerialWrite((const char*)&packet, sizeof(packet));
    }

    return 0;
}


VOID WINAPI SvcMain(DWORD dwArgc, LPTSTR *lpszArgv)
{
    ctx.gSvcStatusHandle = RegisterServiceCtrlHandler(SVC_NAME, SvcCtrlHandler);
    if (!ctx.gSvcStatusHandle)
    {
        SvcReportEvent(TEXT("RegisterServiceCtrlHandler"));
        return;
    }

    ctx.gSvcStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    ctx.gSvcStatus.dwServiceSpecificExitCode = 0;

    ReportSvcStatus(SERVICE_START_PENDING, NO_ERROR, 10000);

    SvcInit(dwArgc, lpszArgv);
}

VOID SvcInit(DWORD dwArgc, LPTSTR *lpszArgv)
{
    ctx.ghSvcStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (!ctx.ghSvcStopEvent)
    {
        ReportSvcStatus(SERVICE_STOPPED, GetLastError(), 0);
        return;
    }

    HANDLE hThread = CreateThread(NULL, 0, ServiceWorkerThread, NULL, 0, NULL);
    if (!hThread)
    {
        ReportSvcStatus(SERVICE_STOPPED, GetLastError(), 0);
        CloseHandle(ctx.ghSvcStopEvent);
        return;
    }

    ReportSvcStatus(SERVICE_RUNNING, NO_ERROR, 0);

    WaitForSingleObject(ctx.ghSvcStopEvent, INFINITE);

    WaitForSingleObject(hThread, INFINITE);
    CloseHandle(hThread);
    CloseHandle(ctx.ghSvcStopEvent);

    ReportSvcStatus(SERVICE_STOPPED, NO_ERROR, 0);
}

VOID WINAPI SvcCtrlHandler(DWORD dwCtrl)
{
    switch(dwCtrl)
    {
        case SERVICE_CONTROL_STOP:
            ReportSvcStatus(SERVICE_STOP_PENDING, NO_ERROR, 0);
            SetEvent(ctx.ghSvcStopEvent);
            break;
        case SERVICE_CONTROL_INTERROGATE:
            ReportSvcStatus(ctx.gSvcStatus.dwCurrentState, NO_ERROR, 0);
            break;
        default:
            break;
    }
}

VOID ReportSvcStatus(DWORD dwCurrentState, DWORD dwWin32ExitCode, DWORD dwWaitHint)
{
    static DWORD dwCheckPoint = 1;

    ctx.gSvcStatus.dwCurrentState = dwCurrentState;
    ctx.gSvcStatus.dwWin32ExitCode = dwWin32ExitCode;
    ctx.gSvcStatus.dwWaitHint = dwWaitHint;

    if (dwCurrentState == SERVICE_START_PENDING)
        ctx.gSvcStatus.dwControlsAccepted = 0;
    else
        ctx.gSvcStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;

    if (dwCurrentState == SERVICE_RUNNING || dwCurrentState == SERVICE_STOPPED)
        ctx.gSvcStatus.dwCheckPoint = 0;
    else
        ctx.gSvcStatus.dwCheckPoint = dwCheckPoint++;

    SetServiceStatus(ctx.gSvcStatusHandle, &ctx.gSvcStatus);
}

VOID SvcReportEvent(LPTSTR szFunction)
{
    HANDLE hEventSource = RegisterEventSource(NULL, SVC_NAME);
    if (!hEventSource)
        return;

    TCHAR Buffer[256];
    StringCchPrintf(Buffer, 256, TEXT("%s failed with %lu"), szFunction, GetLastError());

    LPCTSTR lpszStrings[2] = { SVC_NAME, Buffer };
    ReportEvent(hEventSource, EVENTLOG_ERROR_TYPE, 0, 0, NULL, 2, 0, lpszStrings, NULL);

    DeregisterEventSource(hEventSource);
}

VOID SvcInstall()
{
    TCHAR szPath[MAX_PATH];
    if (!GetModuleFileName(NULL, szPath, MAX_PATH))
    {
        printf("Cannot install service (%lu)\n", GetLastError());
        return;
    }

    TCHAR szQuotedPath[MAX_PATH];
    StringCbPrintf(szQuotedPath, MAX_PATH, TEXT("\"%s\""), szPath);

    SC_HANDLE schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!schSCManager)
    {
        printf("OpenSCManager failed (%lu)\n", GetLastError());
        return;
    }

    SC_HANDLE schService = CreateService(
        schSCManager,
        SVC_NAME,
        SVC_NAME,
        SERVICE_ALL_ACCESS,
        SERVICE_WIN32_OWN_PROCESS,
        SERVICE_DEMAND_START,
        SERVICE_ERROR_NORMAL,
        szQuotedPath,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL
    );

    if (!schService)
    {
        printf("CreateService failed (%lu)\n", GetLastError());
    }
    else
    {
        printf("Service installed successfully\n");
        CloseServiceHandle(schService);
    }

    CloseServiceHandle(schSCManager);
}