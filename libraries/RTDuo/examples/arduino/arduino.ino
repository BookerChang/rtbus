/*
 * Native RTDuo application test code for Arduino CLI.
 *
 * The Arduino core provides main(); this sketch talks to the runtime through
 * the RTDuo ABI table installed by the Zephyr-built runtime.
 */

static uint32_t module_count;
static runtime_ble_conn_t ble_conn;

static void onEvent(const struct runtime_event_tlv *event) {
  uint8_t reason;
  uint8_t level;
  uint8_t err;

  if (runtime_event_is_ble_connect_failed(event, &err)) {
    Serial.printf("arduino application: BLE connect failed err=0x%02x\r\n",
                  err);
  }

  if (runtime_event_is_ble_connected(event, &ble_conn)) {
    Serial.println("arduino application: BLE connected");
    (void)runtime_ble_pair(ble_conn);
  }

  if (runtime_event_is_ble_disconnected(event, &reason)) {
    Serial.printf("arduino application: BLE disconnected reason=0x%02x\r\n",
                  reason);
  }

  if (runtime_event_is_ble_recycled(event)) {
    Serial.println("arduino application: BLE recycled");
  }

  if (runtime_event_is_ble_security_changed(event, &level, &err)) {
    Serial.printf("arduino application: BLE security level=%u err=%u\r\n",
                  level, err);
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("arduino application: setup");

  // rtbus_on_event(onEvent);
  // runtime_ble_adv_start();
}

void loop() {
  int ret = 0;

  delay(2000);

  module_count = runtime_diagnostics_add((int32_t)module_count, 2);
  Serial.printf("arduino application: diagnostics add result=%lu\r\n",
                (unsigned long)module_count);

  {
    static const uint8_t payload[] = {0x99};

    (void)payload;
    /* ret = runtime_lorawan_send(2, payload, sizeof(payload), 0); */
  }

  if (ret != 0) {
    Serial.printf("arduino application: loop failed=%d\r\n", ret);
  }
}
