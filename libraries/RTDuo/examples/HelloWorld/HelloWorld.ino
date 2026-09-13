
// static char cli_post_bytes[8][2];
// static size_t cli_post_index;

// static void postCliByte(char ch) {
//   char *slot = cli_post_bytes[cli_post_index];
//   cli_post_index = (cli_post_index + 1U) % (sizeof(cli_post_bytes) / sizeof(cli_post_bytes[0]));

//   slot[0] = ch;
//   slot[1] = '\0';
//   int ret = runtime_cli_post(slot);
//   if (ret != 0) {
//     Serial.printf("\r\ncli post ret=%ld\r\n", (long)ret);
//   }
// }

void setup() {
  Serial.begin(115200);
  Serial1.begin(115200);
  Serial.println("RTDuo serial echo + @RTBUS CLI333");

  pinMode(IO0, OUTPUT);
  pinMode(IO1, OUTPUT);
  digitalWrite(IO0, HIGH);
  digitalWrite(IO1, HIGH);

}
//@RTBUS:TEST
//@RTBUS:DFU=APP
void loop() {
  while (Serial.available() > 0) {
    int value = Serial.read();

    if (value >= 0) {
      Serial.write((uint8_t)value);
      // postCliByte((char)value);
    }
  }

  delay(10);
}
