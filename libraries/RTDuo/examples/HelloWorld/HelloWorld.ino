// #include "../../../../project/niin/orp/project.ino"
// #include "../../../../project/rakwireless/nbiot/project.ino"
#include "../../../../project/testbench/ble/peripheral.ino"

#ifndef PROJECT_ALONE


void setup() {
  Serial.begin(115200);
  Serial1.begin(115200);
  delay(1000);
  Serial.println("RTDuo serial echo + @RTBUS CLI");

  pinMode(LED0, OUTPUT);
  pinMode(LED1, OUTPUT);
  digitalWrite(LED0, HIGH);
  digitalWrite(LED1, HIGH);

  
  pinMode(AIN1, OUTPUT);
  digitalWrite(AIN1, LOW);
  pinMode(IO2, OUTPUT);
  digitalWrite(IO2, LOW);
}

void loop() {
  while (Serial.available() > 0) {
    int value = Serial.read();

    if (value >= 0) {
      Serial.write((uint8_t)value);
    }
  }

  delay(10);
}

#endif
