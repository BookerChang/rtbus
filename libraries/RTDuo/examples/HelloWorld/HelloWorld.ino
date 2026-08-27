
static int32_t module_count;

void setup() {
  Serial.begin(115200);
  Serial.println("Hello RTDuo");
}

void loop() {
  module_count = runtime_diagnostics_add((int32_t)module_count, 2);
  Serial.printf("arduino application: diagnostics add result=%lu\r\n",
                (unsigned long)module_count);


  delay(2000);
}
