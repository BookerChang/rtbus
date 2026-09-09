#include "HardwareSerial.h"

#include <stdarg.h>

#include <runtime_api.h>

extern "C" void __attribute__((weak)) runtime_arduino_serial_begin(unsigned long baud) {
    (void)baud;
}

extern "C" void __attribute__((weak)) runtime_arduino_serial_end(void) {
}

extern "C" size_t __attribute__((weak)) runtime_arduino_serial_write(const uint8_t *data,
                                                                       size_t size) {
    if (data == nullptr || size == 0) {
        return 0;
    }

    return rtbus_serial_write(data, size);
}

HardwareSerial Serial;

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
    runtime_arduino_serial_begin(baud);
}

void HardwareSerial::end(void) {
    runtime_arduino_serial_end();
}

int HardwareSerial::available(void) {
    return 0;
}

int HardwareSerial::peek(void) {
    return -1;
}

int HardwareSerial::read(void) {
    return -1;
}

void HardwareSerial::flush(void) {
}

size_t HardwareSerial::write(uint8_t value) {
    return write(&value, 1);
}

size_t HardwareSerial::write(const uint8_t *buffer, size_t size) {
    if (buffer == nullptr || size == 0) {
        return 0;
    }

    return runtime_arduino_serial_write(buffer, size);
}

size_t HardwareSerial::printf(const char *fmt, ...) {
    va_list args;
    size_t written = 0;

    if (fmt == nullptr) {
        return 0;
    }

    va_start(args, fmt);
    written = rtbus_serial_vprintf(fmt, args);
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
