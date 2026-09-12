#include "device_wait.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct {
    unsigned int open_attempts;
    unsigned int waits[8];
    size_t wait_count;
    bool stopped;
} FakeDevice;

static int open_after_two_failures(void *context) {
    FakeDevice *device = context;
    device->open_attempts++;
    return device->open_attempts >= 3 ? 42 : -1;
}

static int never_open(void *context) {
    FakeDevice *device = context;
    device->open_attempts++;
    return -1;
}

static bool is_stopped(void *context) {
    return ((FakeDevice *)context)->stopped;
}

static void record_wait(unsigned int delay_ms, void *context) {
    FakeDevice *device = context;
    device->waits[device->wait_count++] = delay_ms;
}

static void stop_during_wait(unsigned int delay_ms, void *context) {
    record_wait(delay_ms, context);
    ((FakeDevice *)context)->stopped = true;
}

int main(void) {
    assert(device_retry_delay_ms(0) == 500);
    assert(device_retry_delay_ms(1) == 1000);
    assert(device_retry_delay_ms(2) == 2000);
    assert(device_retry_delay_ms(3) == 4000);
    assert(device_retry_delay_ms(4) == 5000);
    assert(device_retry_delay_ms(20) == 5000);

    FakeDevice eventually_available = {0};
    assert(wait_for_device(open_after_two_failures, is_stopped, record_wait,
                           &eventually_available) == 42);
    assert(eventually_available.open_attempts == 3);
    assert(eventually_available.wait_count == 2);
    assert(eventually_available.waits[0] == 500);
    assert(eventually_available.waits[1] == 1000);

    FakeDevice interrupted = {0};
    assert(wait_for_device(never_open, is_stopped, stop_during_wait, &interrupted) == -1);
    assert(interrupted.open_attempts == 1);
    assert(interrupted.wait_count == 1);
    return 0;
}
