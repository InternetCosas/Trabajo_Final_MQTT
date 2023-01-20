import paho.mqtt.client as mqtt
import serial
import re

ser = serial.Serial('/dev/cu.usbmodem14201', 9600)

# Conectarse al servidor Mosquitto local
client = mqtt.Client()
client.connect("localhost", 1883, 60)

while True:
    data = ser.readline().decode()
    data = data.replace("\n", "")

    luminosidad = re.compile('Remote brightness measurement:*')
    distancia = re.compile('Remote ultrasound measurement:*')
    temperatura = re.compile('Remote thermistor measurement:*')
    incidenciaLuz = re.compile('Remote direct light measurement:*')

    if luminosidad.match(data):
        medida = data.split()[3] + " " + data.split()[4]
        print(medida)
        client.publish("Luminosidad", medida)
    else:
        if distancia.match(data):
            medida = data.split()[3] + " " + data.split()[4]
            print(medida)
            client.publish("Distancia", medida)
        else:
            if temperatura.match(data):
                medida = data.split()[3] + " " +  data.split()[4]
                print(medida)
                client.publish("Temperatura", medida)
            else:
                if incidenciaLuz.match(data):
                    medida = data.split()[4] + " "  + data.split()[5]
                    print(medida)
                    client.publish("Incidencia_Luz", medida)