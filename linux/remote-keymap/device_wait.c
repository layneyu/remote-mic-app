#include "device_wait.h"

unsigned int device_retry_delay_ms(unsigned int failed_attempts) {
    static const unsigned int delays_ms[] = {500, 1000, 2000, 4000, 5000};
    const unsigned int last = (unsigned int)(sizeof(delays_ms) / sizeof(delays_ms[0]) - 1);
    return delays_ms[failed_attempts < last ? failed_attempts : last];
}

int wait_for_device(
    DeviceOpenCallback open_device,
    DeviceStopCallback stop_requested,
    DeviceWaitCallback wait_before_retry,
    void *context
) {
    unsigned int failed_attempts = 0;
    while (!stop_requested(context)) {
        int descriptor = open_device(context);
        if (descriptor >= 0) {
            return descriptor;
        }
        wait_before_retry(device_retry_delay_ms(failed_attempts), context);
        if (failed_attempts < 4) {
            failed_attempts++;
        }
    }
    return -1;
}
