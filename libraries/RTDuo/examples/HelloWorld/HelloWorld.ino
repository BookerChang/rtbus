// #include "../../../../project/niin/orp/project.ino"
// #include "../../../../project/rakwireless/nbiot/project.ino"

#ifndef PROJECT_ALONE

static runtime_ble_conn_t ble_conn;

static void onEvent(const struct runtime_event_tlv *event) {
  uint8_t reason;
  uint8_t level;
  uint8_t err;

  if (runtime_event_is_ble_connect_failed(event, &err)) {
    Serial.printf("HelloWorld: BLE connect failed err=0x%02x\r\n", err);
  }

  if (runtime_event_is_ble_connected(event, &ble_conn)) {
    Serial.println("HelloWorld: BLE connected");
    (void)runtime_ble_pair(ble_conn);
  }

  if (runtime_event_is_ble_disconnected(event, &reason)) {
    Serial.printf("HelloWorld: BLE disconnected reason=0x%02x\r\n", reason);
  }

  if (runtime_event_is_ble_recycled(event)) {
    Serial.println("HelloWorld: BLE recycled");
    runtime_ble_adv_start();
  }

  if (runtime_event_is_ble_security_changed(event, &level, &err)) {
    Serial.printf("HelloWorld: BLE security level=%u err=%u\r\n", level, err);
  }
}

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

  rtbus_on_event(onEvent);
  runtime_ble_adv_start();
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
