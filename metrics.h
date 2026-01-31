//
// Created by USER on 28/01/2026.
//

#ifndef SERVICE_METRICS_H
#define SERVICE_METRICS_H
typedef struct {
    double current_cpu_usage;
    double current_ram_usage;
    float current_gpu_fan_speed;
    float current_gpu_utilisation;
} DeviceMetrics;
void PollDeviceMetrics(DeviceMetrics *metrics);
#endif //SERVICE_METRICS_H