#pragma once

#include <stddef.h>
#include <stdint.h>

#include "HardwareSerial.h"
#include "RTDuo.h"
#include "pins_arduino.h"

#define LOW 0x0
#define HIGH 0x1
#define INPUT RUNTIME_GPIO_MODE_INPUT
#define OUTPUT RUNTIME_GPIO_MODE_OUTPUT
#define INPUT_PULLUP RUNTIME_GPIO_MODE_INPUT_PULLUP
#define INPUT_PULLDOWN RUNTIME_GPIO_MODE_INPUT_PULLDOWN

#ifdef __cplusplus
extern "C" {
#endif

void delay(unsigned long ms);
unsigned long millis(void);
void pinMode(uint32_t pin, uint32_t mode);
void digitalWrite(uint32_t pin, uint32_t value);
int digitalRead(uint32_t pin);

#ifdef __cplusplus
}

void setup(void);
void loop(void);
#endif
