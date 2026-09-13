#pragma once

#include <stddef.h>
#include <stdint.h>

#if defined(__GNUC__)
#define RTDUO_SERIAL_PRINTF_FORMAT __attribute__((format(printf, 2, 3)))
#else
#define RTDUO_SERIAL_PRINTF_FORMAT
#endif

class HardwareSerial {
public:
    enum Port {
        PortSerial = 0,
        PortSerial1 = 1,
    };

    constexpr explicit HardwareSerial(Port port = PortSerial) : port_(port) {
    }

    void begin(unsigned long baud);
    void end(void);

    int available(void);
    int peek(void);
    int read(void);
    size_t readBytes(char *buffer, size_t length);
    size_t readBytes(uint8_t *buffer, size_t length);
    void flush(void);

    size_t write(uint8_t value);
    size_t write(const uint8_t *buffer, size_t size);

    size_t printf(const char *fmt, ...) RTDUO_SERIAL_PRINTF_FORMAT;
    size_t print(const char *text);
    size_t print(int value);
    size_t print(unsigned int value);
    size_t print(long value);
    size_t print(unsigned long value);
    size_t println(const char *text);
    size_t println(int value);
    size_t println(unsigned int value);
    size_t println(long value);
    size_t println(unsigned long value);
    size_t println(void);

private:
    const Port port_;
};

extern HardwareSerial Serial;
extern HardwareSerial Serial1;

#undef RTDUO_SERIAL_PRINTF_FORMAT
