#include <PubSubClient.h>
#include <Ethernet.h>

const char* mqttServer = "localhost";

EthernetClient ethClient;
PubSubClient mqttClient(ethClient);

void setup() {
  Serial.begin(115200);
  mqttClient.setServer(mqttServer, 1883);
}

void loop() {
  // Realiza la conexión al servidor MQTT
  if (!mqttClient.connected()) {
    Serial.println("Intentando conectarse al servidor MQTT...");
    if (mqttClient.connect("ArduinoClient")) {
      Serial.println("Conectado al servidor MQTT");
    } else {
      Serial.println("Error al conectarse al servidor MQTT");
    }
  }

  // Publica el mensaje "Hola mundo" en el tópico "prueba"
  mqttClient.publish("prueba", "Hola mundo");
  Serial.println("Mensaje publicado en el tópico 'prueba'");

  delay(5000);
}


