import paho.mqtt.client as mqtt
import serial


# Define la función de callback para procesar los mensajes recibidos
def on_message(client, userdata, msg):
    ser = serial.Serial('/dev/cu.usbmodem14201', 9600)
    substring = str(msg.payload).split("'")
    substring = substring[1] + "\n"
    print(substring)
    ser.write(substring.encode())  # send the message "Hello, Arduino!" to the Arduino

# Crea una instancia del cliente MQTT
client = mqtt.Client()


# Conecta al servidor MQTT
client.connect("localhost", 1883)

# Suscribe al tópico deseado
client.subscribe("Suscriptor", qos=1)

# Asigna la función de callback al evento de llegada de mensajes
client.on_message = on_message

# Ejecuta el bucle para recibir mensajes
client.loop_forever()



