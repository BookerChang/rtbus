#include <stdint.h>

#include <runtime_api.h>

extern "C" void pinMode(uint32_t pin, uint32_t mode) {
    (void)rtbus_gpio_configure(pin, mode);
}

extern "C" void digitalWrite(uint32_t pin, uint32_t value) {
    (void)rtbus_gpio_write(pin, value);
}

extern "C" int digitalRead(uint32_t pin) {
    return rtbus_gpio_read(pin) > 0 ? 1 : 0;
}
