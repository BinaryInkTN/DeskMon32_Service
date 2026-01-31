//
// metrics_service.c
//

#include <windows.h>

/* ---- NVML minimal types ---- */
typedef int nvmlReturn_t;
typedef struct nvmlDevice_st* nvmlDevice_t;

#define NVML_SUCCESS 0
#define NVML_TEMPERATURE_GPU 0

typedef struct {
    unsigned int gpu;
    unsigned int memory;
} nvmlUtilization_t;

/* ---- NVML function pointers ---- */
static HMODULE nvml_dll = NULL;
static nvmlDevice_t nvml_device = NULL;

static nvmlReturn_t (*p_nvmlInit)(void);
static nvmlReturn_t (*p_nvmlShutdown)(void);
static nvmlReturn_t (*p_nvmlDeviceGetHandleByIndex)(unsigned int, nvmlDevice_t*);
static nvmlReturn_t (*p_nvmlDeviceGetFanSpeed)(nvmlDevice_t, unsigned int*);
static nvmlReturn_t (*p_nvmlDeviceGetUtilizationRates)(nvmlDevice_t, nvmlUtilization_t*);
static int InitNVML(void)
{
    if (nvml_dll)
        return 1;

    nvml_dll = LoadLibraryA("nvml.dll");
    if (!nvml_dll)
        return 0;

    p_nvmlInit =
        (void*)GetProcAddress(nvml_dll, "nvmlInit_v2");
    p_nvmlShutdown =
        (void*)GetProcAddress(nvml_dll, "nvmlShutdown");
    p_nvmlDeviceGetHandleByIndex =
        (void*)GetProcAddress(nvml_dll, "nvmlDeviceGetHandleByIndex_v2");
    p_nvmlDeviceGetFanSpeed =
        (void*)GetProcAddress(nvml_dll, "nvmlDeviceGetFanSpeed");
    p_nvmlDeviceGetUtilizationRates =
        (void*)GetProcAddress(nvml_dll, "nvmlDeviceGetUtilizationRates");

    if (!p_nvmlInit ||
        !p_nvmlShutdown ||
        !p_nvmlDeviceGetHandleByIndex ||
        !p_nvmlDeviceGetFanSpeed ||
        !p_nvmlDeviceGetUtilizationRates)
        return 0;

    if (p_nvmlInit() != NVML_SUCCESS)
        return 0;

    if (p_nvmlDeviceGetHandleByIndex(0, &nvml_device) != NVML_SUCCESS)
        return 0;

    return 1;
}

#include <stdio.h>
#include "metrics.h"


static ULARGE_INTEGER last_idle = {0}, last_kernel = {0}, last_user = {0};

double GetCPUUsage()
{
    FILETIME idleTime, kernelTime, userTime;
    if (!GetSystemTimes(&idleTime, &kernelTime, &userTime))
        return 0.0;

    ULARGE_INTEGER idle, kernel, user;
    idle.LowPart = idleTime.dwLowDateTime; idle.HighPart = idleTime.dwHighDateTime;
    kernel.LowPart = kernelTime.dwLowDateTime; kernel.HighPart = kernelTime.dwHighDateTime;
    user.LowPart = userTime.dwLowDateTime; user.HighPart = userTime.dwHighDateTime;

    ULONGLONG sys_idle = idle.QuadPart - last_idle.QuadPart;
    ULONGLONG sys_kernel = kernel.QuadPart - last_kernel.QuadPart;
    ULONGLONG sys_user = user.QuadPart - last_user.QuadPart;
    ULONGLONG sys_total = sys_kernel + sys_user;

    double cpu = 0.0;
    if (sys_total > 0)
        cpu = (double)(sys_total - sys_idle) * 100.0 / sys_total;

    last_idle = idle;
    last_kernel = kernel;
    last_user = user;

    return cpu;
}


double GetRAMUsage()
{
    MEMORYSTATUSEX mem = {0};
    mem.dwLength = sizeof(mem);

    if (GlobalMemoryStatusEx(&mem)) {
        return (double)mem.dwMemoryLoad; // 0..100%
    }

    return 0.0;
}

float GetGPUFanSpeed()
{
    if (!InitNVML() || !nvml_device)
        return 0.0f;

    unsigned int fan = 0;
    if (p_nvmlDeviceGetFanSpeed(nvml_device, &fan) != NVML_SUCCESS)
        return 0.0f;

    return (float)fan; // percent
}


float GetGPUUsage()
{
    if (!InitNVML() || !nvml_device)
        return 0.0f;

    nvmlUtilization_t util = {0};
    if (p_nvmlDeviceGetUtilizationRates(nvml_device, &util) != NVML_SUCCESS)
        return 0.0f;

    return (float)util.gpu; // percent
}

void PollDeviceMetrics(DeviceMetrics *metrics)
{
    if (!metrics) return;

    metrics->current_cpu_usage = GetCPUUsage();
    metrics->current_ram_usage = GetRAMUsage();
    metrics->current_gpu_fan_speed = GetGPUFanSpeed();
    metrics->current_gpu_utilisation = GetGPUUsage();
}
