import paho.mqtt.client as mqtt
import serial

ser = serial.Serial('/dev/cu.usbmodem14201', 9600)

while True:
    # Leer desde el puerto serie lo que se está mandando desde el Arduino
    data = ser.readline().decode()

    # Conectarse al servidor Mosquitto local
    client = mqtt.Client()
    client.connect("localhost", 1883, 60)

    # Enviar el mensaje obtenido desde el puerto serie al tópico "MQTT_IC"
    client.publish("MQTT_IC", data)

    # Desconectarse del servidor Mosquitto
    client.disconnect()