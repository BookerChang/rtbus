#include <stdint.h>

#include <runtime_api.h>

extern "C" void pinMode(uint32_t pin, uint32_t mode) {
    runtime_gpio_configure_api_t gpio_configure;

    gpio_configure = WZ_API_FN(WISNODEZ_API_SLOT_GPIO_CONFIGURE,
                               runtime_gpio_configure_api_t);
    if (gpio_configure == nullptr) {
        return;
    }

    (void)gpio_configure(pin, mode);
}

extern "C" void digitalWrite(uint32_t pin, uint32_t value) {
    runtime_gpio_write_api_t gpio_write;

    gpio_write = WZ_API_FN(WISNODEZ_API_SLOT_GPIO_WRITE,
                           runtime_gpio_write_api_t);
    if (gpio_write == nullptr) {
        return;
    }

    (void)gpio_write(pin, value);
}

extern "C" int digitalRead(uint32_t pin) {
    runtime_gpio_read_api_t gpio_read;

    gpio_read = WZ_API_FN(WISNODEZ_API_SLOT_GPIO_READ,
                          runtime_gpio_read_api_t);
    if (gpio_read == nullptr) {
        return 0;
    }

    return gpio_read(pin) > 0 ? 1 : 0;
}
