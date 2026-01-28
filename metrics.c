//
// metrics_service.c
//

#include <windows.h>
#include <stdio.h>

typedef struct {
    double current_cpu_usage;
    double current_ram_usage;
} DeviceMetrics;

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

void PollDeviceMetrics(DeviceMetrics *metrics)
{
    if (!metrics) return;

    metrics->current_cpu_usage = GetCPUUsage();
    metrics->current_ram_usage = GetRAMUsage();
}
