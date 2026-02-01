#define _WIN32_DCOM
#include <windows.h>
#include <wbemidl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "metrics.h"

#pragma comment(lib, "wbemuuid.lib")

/* ===================== NVML DEFINITIONS ===================== */

typedef int nvmlReturn_t;
typedef struct nvmlDevice_st* nvmlDevice_t;

#define NVML_SUCCESS 0
#define NVML_TEMPERATURE_GPU 0

typedef struct {
    unsigned int gpu;
    unsigned int memory;
} nvmlUtilization_t;

/* ===================== STATIC STATE ===================== */

static INIT_ONCE g_nvml_init_once = INIT_ONCE_STATIC_INIT;
static HMODULE g_nvml_dll = NULL;
static nvmlDevice_t g_nvml_device = NULL;
/* Function pointers */
static nvmlReturn_t (*p_nvmlInit)(void);
static nvmlReturn_t (*p_nvmlShutdown)(void);
static nvmlReturn_t (*p_nvmlDeviceGetHandleByIndex)(unsigned int, nvmlDevice_t*);
static nvmlReturn_t (*p_nvmlDeviceGetFanSpeed)(nvmlDevice_t, unsigned int*);
static nvmlReturn_t (*p_nvmlDeviceGetUtilizationRates)(nvmlDevice_t, nvmlUtilization_t*);
static nvmlReturn_t (*p_nvmlDeviceGetTemperature)(nvmlDevice_t, unsigned int, unsigned int*);

static BOOL CALLBACK InitNVMLOnce(PINIT_ONCE once, PVOID param, PVOID *ctx)
{
    (void)once; (void)param; (void)ctx;

    g_nvml_dll = LoadLibraryA("nvml.dll");
    if (!g_nvml_dll)
        return FALSE;

    p_nvmlInit  = (void*)GetProcAddress(g_nvml_dll, "nvmlInit_v2");
    p_nvmlShutdown = (void*)GetProcAddress(g_nvml_dll, "nvmlShutdown");
    p_nvmlDeviceGetHandleByIndex =
        (void*)GetProcAddress(g_nvml_dll, "nvmlDeviceGetHandleByIndex_v2");
    p_nvmlDeviceGetFanSpeed =
        (void*)GetProcAddress(g_nvml_dll, "nvmlDeviceGetFanSpeed");
    p_nvmlDeviceGetUtilizationRates =
        (void*)GetProcAddress(g_nvml_dll, "nvmlDeviceGetUtilizationRates");
    p_nvmlDeviceGetTemperature =
        (void*)GetProcAddress(g_nvml_dll, "nvmlDeviceGetTemperature");

    if (!p_nvmlInit ||
        !p_nvmlShutdown ||
        !p_nvmlDeviceGetHandleByIndex ||
        !p_nvmlDeviceGetFanSpeed ||
        !p_nvmlDeviceGetUtilizationRates ||
        !p_nvmlDeviceGetTemperature)
        return FALSE;

    if (p_nvmlInit() != NVML_SUCCESS)
        return FALSE;

    if (p_nvmlDeviceGetHandleByIndex(0, &g_nvml_device) != NVML_SUCCESS)
        return FALSE;

    return TRUE;
}
static bool EnsureNVML(void)
{
    return InitOnceExecuteOnce(&g_nvml_init_once, InitNVMLOnce, NULL, NULL);
}

/* ===================== CPU USAGE ===================== */

static ULARGE_INTEGER last_idle = {0}, last_kernel = {0}, last_user = {0};
static bool cpu_initialized = false;

static double GetCPUUsage(void)
{
    FILETIME idleTime, kernelTime, userTime;

    if (!GetSystemTimes(&idleTime, &kernelTime, &userTime))
        return 0.0;

    ULARGE_INTEGER idle, kernel, user;
    idle.LowPart   = idleTime.dwLowDateTime;
    idle.HighPart  = idleTime.dwHighDateTime;
    kernel.LowPart = kernelTime.dwLowDateTime;
    kernel.HighPart= kernelTime.dwHighDateTime;
    user.LowPart   = userTime.dwLowDateTime;
    user.HighPart  = userTime.dwHighDateTime;

    if (!cpu_initialized) {
        last_idle = idle;
        last_kernel = kernel;
        last_user = user;
        cpu_initialized = true;
        return 0.0;
    }

    ULONGLONG idle_delta   = idle.QuadPart   - last_idle.QuadPart;
    ULONGLONG kernel_delta = kernel.QuadPart - last_kernel.QuadPart;
    ULONGLONG user_delta   = user.QuadPart   - last_user.QuadPart;

    last_idle = idle;
    last_kernel = kernel;
    last_user = user;

    ULONGLONG total = kernel_delta + user_delta;
    if (total == 0)
        return 0.0;

    return (double)(total - idle_delta) * 100.0 / total;
}

/* ===================== MEMORY ===================== */

static double GetRAMUsage(void)
{
    MEMORYSTATUSEX mem = { sizeof(mem) };
    if (!GlobalMemoryStatusEx(&mem))
        return 0.0;

    return (double)mem.dwMemoryLoad;
}

/* ===================== GPU ===================== */

static float GetGPUFanSpeed(void)
{
    if (!EnsureNVML())
        return 0.0f;

    unsigned int fan = 0;
    if (p_nvmlDeviceGetFanSpeed(g_nvml_device, &fan) != NVML_SUCCESS)
        return 0.0f;

    return (float)fan;
}

static float GetGPUUsage(void)
{
    if (!EnsureNVML())
        return 0.0f;

    nvmlUtilization_t util = {0};
    if (p_nvmlDeviceGetUtilizationRates(g_nvml_device, &util) != NVML_SUCCESS)
        return 0.0f;

    return (float)util.gpu;
}

static float GetGPUTemperature(void)
{
    if (!EnsureNVML())
        return 0.0f;

    unsigned int temp = 0;
    if (p_nvmlDeviceGetTemperature(g_nvml_device, NVML_TEMPERATURE_GPU, &temp) != NVML_SUCCESS)
        return 0.0f;

    return (float)temp;
}

/* ===================== WMI HELPERS ===================== */

static bool InitCOM(void)
{
    static INIT_ONCE once = INIT_ONCE_STATIC_INIT;
    static HRESULT result = E_FAIL;

    BOOL CALLBACK Init(PINIT_ONCE o, PVOID p, PVOID *c) {
        (void)o; (void)p; (void)c;
        result = CoInitializeEx(NULL, COINIT_MULTITHREADED);
        return SUCCEEDED(result) || result == RPC_E_CHANGED_MODE;
    }

    InitOnceExecuteOnce(&once, Init, NULL, NULL);
    return SUCCEEDED(result) || result == RPC_E_CHANGED_MODE;
}

/* ===================== CPU TEMP ===================== */

static float GetCPUTemperature(void)
{
    if (!InitCOM())
        return 0.0f;

    IWbemLocator *loc = NULL;
    IWbemServices *svc = NULL;
    IEnumWbemClassObject *en = NULL;
    IWbemClassObject *obj = NULL;
    ULONG ret = 0;
    float temp = 0.0f;

    if (FAILED(CoCreateInstance(&CLSID_WbemLocator, NULL, CLSCTX_INPROC_SERVER,
                                &IID_IWbemLocator, (void**)&loc)))
        goto cleanup;

    if (FAILED(loc->lpVtbl->ConnectServer(
        loc, L"ROOT\\WMI", NULL, NULL, NULL, 0, NULL, NULL, &svc)))
        goto cleanup;
    CoSetProxyBlanket(
        (IUnknown*)svc,
        RPC_C_AUTHN_WINNT,
        RPC_C_AUTHZ_NONE,
        NULL,
        RPC_C_AUTHN_LEVEL_CALL,
        RPC_C_IMP_LEVEL_IMPERSONATE,
        NULL,
        EOAC_NONE
    );

    if (FAILED(svc->lpVtbl->ExecQuery(
        svc, L"WQL",
        L"SELECT CurrentTemperature FROM MSAcpi_ThermalZoneTemperature",
        WBEM_FLAG_FORWARD_ONLY, NULL, &en)))
        goto cleanup;

    if (en->lpVtbl->Next(en, WBEM_INFINITE, 1, &obj, &ret) == S_OK) {
        VARIANT v;
        VariantInit(&v);
        if (SUCCEEDED(obj->lpVtbl->Get(obj, L"CurrentTemperature", 0, &v, NULL, NULL)))
            temp = ((float)v.uintVal / 10.0f) - 273.15f;
        VariantClear(&v);
    }

cleanup:
    if (obj) obj->lpVtbl->Release(obj);
    if (en) en->lpVtbl->Release(en);
    if (svc) svc->lpVtbl->Release(svc);
    if (loc) loc->lpVtbl->Release(loc);
    return temp;
}

/* ===================== DRIVES ===================== */

static void GetDriveSpaceUsage(DeviceMetrics *m)
{
    DWORD mask = GetLogicalDrives();
    char root[] = "A:\\";
    int count = 0;

    for (int i = 0; i < 26 && count < MAX_DRIVES; ++i) {
        if (!(mask & (1 << i)))
            continue;

        root[0] = (char)('A' + i);
        if (GetDriveTypeA(root) != DRIVE_FIXED)
            continue;

        ULARGE_INTEGER free, total, total_free;
        if (!GetDiskFreeSpaceExA(root, &free, &total, &total_free))
            continue;

        float total_gb = (float)total.QuadPart / (1024.0f * 1024.0f * 1024.0f);
        float free_gb  = (float)total_free.QuadPart / (1024.0f * 1024.0f * 1024.0f);

        m->drive_letters[count] = root[0];
        m->drive_total_gb[count] = total_gb;
        m->drive_free_gb[count]  = free_gb;
        m->drive_used_percent[count] =
            total_gb > 0.0f ? ((total_gb - free_gb) / total_gb) * 100.0f : 0.0f;

        ++count;
    }

    m->drive_count = count;
}

/* ===================== PUBLIC API ===================== */

void PollDeviceMetrics(DeviceMetrics *m)
{
    if (!m) return;

    m->current_cpu_usage        = GetCPUUsage();
    m->current_ram_usage        = GetRAMUsage();
    m->current_cpu_temperature  = GetCPUTemperature();
    m->current_gpu_temperature  = GetGPUTemperature();
    m->current_gpu_utilisation  = GetGPUUsage();
    m->current_gpu_fan_speed    = GetGPUFanSpeed();

    GetDriveSpaceUsage(m);
}
