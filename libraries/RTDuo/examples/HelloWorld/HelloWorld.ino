// #include "../../../../project/niin/orp/project.ino"
// #include "../../../../project/rakwireless/nbiot/project.ino"

#ifndef PROJECT_ALONE

void setup() {
  Serial.begin(115200);
  Serial1.begin(115200);
  Serial.println("RTDuo serial echo + @RTBUS CLI333");

  pinMode(LED0, OUTPUT);
  pinMode(LED1, OUTPUT);
  digitalWrite(LED0, HIGH);
  digitalWrite(LED1, HIGH);

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
