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
    void begin(unsigned long baud);
    void end(void);

    int available(void);
    int peek(void);
    int read(void);
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
};

extern HardwareSerial Serial;

#undef RTDUO_SERIAL_PRINTF_FORMAT
