#ifndef SERVICE_METRICS_H
#define SERVICE_METRICS_H

#include <stdint.h>

#define MAX_DRIVES 10
#define MAX_NAME_LEN 128  // For CPU/GPU short names

typedef struct {
    double current_cpu_usage;
    double current_ram_usage;

    float  current_cpu_temperature;
    float  current_cpu_fan_speed;

    float  current_gpu_fan_speed;
    float  current_gpu_utilisation;
    float  current_gpu_temperature;

    char   cpu_name[MAX_NAME_LEN];  // Short CPU name (e.g., "Ryzen 5 5600X")
    char   gpu_name[MAX_NAME_LEN];  // Short GPU name (e.g., "RTX 3060")

    char   drive_letters[MAX_DRIVES];
    double drive_used_percent[MAX_DRIVES];
    double drive_total_gb[MAX_DRIVES];
    double drive_free_gb[MAX_DRIVES];
    uint8_t drive_count;
} DeviceMetrics;

void PollDeviceMetrics(DeviceMetrics *metrics);

#endif // SERVICE_METRICS_H
