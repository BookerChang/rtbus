#include "HardwareSerial.h"

#include <stdarg.h>

#include <runtime_api.h>

extern "C" size_t __attribute__((weak)) runtime_arduino_serial_write(uint32_t port,
                                                                     const uint8_t *data,
                                                                     size_t size) {
    if (data == nullptr || size == 0) {
        return 0;
    }

    return rtbus_serial_write(port, data, size);
}

extern "C" int __attribute__((weak)) runtime_arduino_serial_begin(uint32_t port,
                                                                  uint32_t baud) {
    return runtime_serial_begin(port, baud);
}

extern "C" size_t __attribute__((weak)) runtime_arduino_serial_read(uint32_t port,
                                                                    uint8_t *data,
                                                                    size_t size) {
    size_t received = 0;

    if (data == nullptr || size == 0) {
        return 0;
    }

    while (received < size) {
        int32_t value = rtbus_serial_read(port);

        if (value < 0) {
            break;
        }

        data[received++] = static_cast<uint8_t>(value);
    }

    return received;
}

HardwareSerial Serial(HardwareSerial::PortSerial);
HardwareSerial Serial1(HardwareSerial::PortSerial1);

struct SerialInstance {
    uint32_t port;
    int peeked;
};

static SerialInstance serial_instances[] = {
    [HardwareSerial::PortSerial] = {
        .port = RTBUS_SERIAL_PORT_0,
        .peeked = -1,
    },
    [HardwareSerial::PortSerial1] = {
        .port = RTBUS_SERIAL_PORT_1,
        .peeked = -1,
    },
};

static size_t string_length(const char *text) {
    size_t length = 0;

    if (text == nullptr) {
        return 0;
    }

    while (text[length] != '\0') {
        ++length;
    }

    return length;
}

static size_t serial_print_unsigned(HardwareSerial *serial, unsigned long value) {
    char digits[10];
    size_t length = 0;
    size_t written = 0;

    do {
        digits[length++] = static_cast<char>('0' + (value % 10));
        value /= 10;
    } while (value != 0 && length < sizeof(digits));

    while (length > 0) {
        written += serial->write(static_cast<uint8_t>(digits[--length]));
    }

    return written;
}

void HardwareSerial::begin(unsigned long baud) {
    SerialInstance *instance = &serial_instances[static_cast<size_t>(port_)];

    (void)runtime_arduino_serial_begin(instance->port,
                                       static_cast<uint32_t>(baud));
}

void HardwareSerial::end(void) {
}

int HardwareSerial::available(void) {
    uint8_t byte;
    SerialInstance *instance = &serial_instances[static_cast<size_t>(port_)];

    if (instance->peeked >= 0) {
        return 1;
    }

    if (runtime_arduino_serial_read(instance->port, &byte, 1) == 0) {
        return 0;
    }

    instance->peeked = byte;
    return 1;
}

int HardwareSerial::peek(void) {
    if (available() == 0) {
        return -1;
    }

    SerialInstance *instance = &serial_instances[static_cast<size_t>(port_)];

    return instance->peeked;
}

int HardwareSerial::read(void) {
    uint8_t byte;
    SerialInstance *instance = &serial_instances[static_cast<size_t>(port_)];

    if (instance->peeked >= 0) {
        int value = instance->peeked;

        instance->peeked = -1;
        return value;
    }

    if (runtime_arduino_serial_read(instance->port, &byte, 1) == 0) {
        return -1;
    }

    return byte;
}

size_t HardwareSerial::readBytes(char *buffer, size_t length) {
    return readBytes(reinterpret_cast<uint8_t *>(buffer), length);
}

size_t HardwareSerial::readBytes(uint8_t *buffer, size_t length) {
    size_t received = 0;

    if (buffer == nullptr || length == 0) {
        return 0;
    }

    while (received < length) {
        int value = read();

        if (value < 0) {
            break;
        }

        buffer[received++] = static_cast<uint8_t>(value);
    }

    return received;
}

void HardwareSerial::flush(void) {
}

size_t HardwareSerial::write(uint8_t value) {
    return write(&value, 1);
}

size_t HardwareSerial::write(const uint8_t *buffer, size_t size) {
    SerialInstance *instance = &serial_instances[static_cast<size_t>(port_)];

    if (buffer == nullptr || size == 0) {
        return 0;
    }

    return runtime_arduino_serial_write(instance->port, buffer, size);
}

size_t HardwareSerial::printf(const char *fmt, ...) {
    va_list args;
    SerialInstance *instance = &serial_instances[static_cast<size_t>(port_)];
    size_t written = 0;

    if (fmt == nullptr) {
        return 0;
    }

    va_start(args, fmt);
    written = rtbus_serial_vprintf(instance->port, fmt, args);
    va_end(args);

    return written;
}

size_t HardwareSerial::print(const char *text) {
    return write(reinterpret_cast<const uint8_t *>(text), string_length(text));
}

size_t HardwareSerial::print(int value) {
    return print(static_cast<long>(value));
}

size_t HardwareSerial::print(unsigned int value) {
    return print(static_cast<unsigned long>(value));
}

size_t HardwareSerial::print(long value) {
    size_t written = 0;
    unsigned long magnitude;

    if (value < 0) {
        written += write(static_cast<uint8_t>('-'));
        magnitude = 0UL - static_cast<unsigned long>(value);
    } else {
        magnitude = static_cast<unsigned long>(value);
    }

    written += serial_print_unsigned(this, magnitude);
    return written;
}

size_t HardwareSerial::print(unsigned long value) {
    return serial_print_unsigned(this, value);
}

size_t HardwareSerial::println(const char *text) {
    size_t written = print(text);
    written += println();
    return written;
}

size_t HardwareSerial::println(int value) {
    size_t written = print(value);
    written += println();
    return written;
}

size_t HardwareSerial::println(unsigned int value) {
    size_t written = print(value);
    written += println();
    return written;
}

size_t HardwareSerial::println(long value) {
    size_t written = print(value);
    written += println();
    return written;
}

size_t HardwareSerial::println(unsigned long value) {
    size_t written = print(value);
    written += println();
    return written;
}

size_t HardwareSerial::println(void) {
    static const uint8_t newline[] = {'\r', '\n'};
    return write(newline, sizeof(newline));
}
