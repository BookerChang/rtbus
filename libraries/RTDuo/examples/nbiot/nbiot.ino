
static int32_t module_count;

static void drain_bg77_response(unsigned int rounds) {
  uint8_t buffer[32];

  while (rounds-- > 0U) {
    size_t received;

    while ((received = Serial1.readBytes(buffer, sizeof(buffer))) > 0U) {
      Serial.write(buffer, received);
    }

    delay(100);
  }
}

void setup() {
  Serial.begin(115200);
  Serial1.begin(115200);
  Serial.println("Hello RTDuo");

  pinMode(IO0, OUTPUT);
  pinMode(IO1, OUTPUT);
  digitalWrite(IO0, HIGH);
  digitalWrite(IO1, HIGH);

  pinMode(IO21, OUTPUT);
  digitalWrite(IO21, HIGH);
  delay(500);
  digitalWrite(IO21, LOW);
  delay(500);

  Serial1.println("ATI");
  drain_bg77_response(30);
}

void loop() {

  drain_bg77_response(1);
  module_count = runtime_diagnostics_add((int32_t)module_count, 2);
  Serial.printf("arduino application: diagnostics add result=%lu\r\n",
                (unsigned long)module_count);

  delay(2000);
}
