#include <stdint.h>

#include "runtime_api.h"

void arduino_setup(void);
void arduino_loop(void);

void delay(unsigned long ms)
{
    (void)k_delay((uint32_t)ms);
}

unsigned long millis(void)
{
    return (unsigned long)rtbus_millis();
}

int main(void)
{
    if (!rtbus_api_is_ready()) {
        return -RTBUS_ENOSYS;
    }

    arduino_setup();

    while (1) {
        arduino_loop();
    }
}
