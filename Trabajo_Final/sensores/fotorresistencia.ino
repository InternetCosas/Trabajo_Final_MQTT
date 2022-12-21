const int LDR_Apin = A0;
const int LDR_Dpin = 2;


void setup() {
  pinMode(LDR_Apin, INPUT);
  pinMode(LDR_Dpin, INPUT);
}


void loop() {
  int d_read = digitalRead(LDR_Dpin);
  int a_read = analogRead(LDR_Apin);
  SerialUSB.println(d_read);
  SerialUSB.println(a_read);
  delay(10000);
}