const int LDRPin = 2;

void setup() {
  pinMode(LDRPin, INPUT);
}


void loop() {
  int value = digitalRead(LDRPin);
  SerialUSB.println(value);
  SerialUSB.println(analogRead(LDRPin));
  delay(1000);
}