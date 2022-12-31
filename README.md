# Trabajo_Final_MQTT

Proyecto en el que se plantea crear una red LoRa compuesta por tres placas Arduino MKR 1310 como sensores/actuadores.
Una cuarta placa Arduino MKR 1310 actuando como concentrador, conectada a un PC que actue como gateway a un servidor MQTT

## Fuentes utilizadas
Fotorresistencia: https://www.luisllamas.es/medir-nivel-luz-con-arduino-y-fotoresistencia-ldr/

Termistor: https://www.luisllamas.es/medir-temperatura-con-arduino-y-termistor-mf52/

LCD1602: https://naylampmechatronics.com/blog/34_tutorial-lcd-conectando-tu-arduino-a-un-lcd1602-y-lcd2004.html


## Como utilizar Mosquitto para comunicaciones vía MQTT a través de línea de comandos
   1. Descargar e instalar Mosquito para tu SO desde la página https://mosquitto.org/download/
   2. Iniciar el servidor Mosquitto, en caso de MacOS se utilizará el comando "Mosquitto -d"
   3. Luego desde la línea de comando, si se ejecuta el comando "mosquitto_sub -t "prueba"" se estará suscribiendo al tópico llamado prueba, la ejecución de este comando hará que esa ventana de la línea de comando ahora se quede escuchando todos los mensajes que le lleguen.
   4. En otra ventana de la linea de comandos, se lanza el comando "mosquitto_pub -t "prueba" -m "Hola mundo"", esto hará se se visualice el mensaje "Hola mundo" en la ventana de la linea de comando en la que se ejecutó la suscripción

