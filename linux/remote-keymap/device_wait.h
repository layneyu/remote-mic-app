#ifndef XIAOMI_REMOTE_DEVICE_WAIT_H
#define XIAOMI_REMOTE_DEVICE_WAIT_H

#include <stdbool.h>

typedef int (*DeviceOpenCallback)(void *context);
typedef bool (*DeviceStopCallback)(void *context);
typedef void (*DeviceWaitCallback)(unsigned int delay_ms, void *context);

unsigned int device_retry_delay_ms(unsigned int failed_attempts);
int wait_for_device(
    DeviceOpenCallback open_device,
    DeviceStopCallback stop_requested,
    DeviceWaitCallback wait_before_retry,
    void *context
);

#endif
