int indice = 0;

void setup() {
  Serial.begin(9600);
}

void loop() {
    Serial.println("Hola, esto está enviado desde Arduino:" + String(indice));
    indice = indice + 1;
    delay(3000);
}