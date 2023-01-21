#!/bin/bash
osascript -e 'tell application "Terminal"
    do script "/usr/local/opt/mosquitto/sbin/mosquitto -c /usr/local/etc/mosquitto/mosquitto.conf"
    tell application "System Events" to keystroke "t" using command down
    delay 0.5
    do script "clear" in front tab of the front window
    delay 0.5
    do script "mosquitto_sub -t \"Luminosidad\"" in front tab of the front window
    tell application "System Events" to keystroke "t" using command down
    delay 0.5
    do script "clear" in front tab of the front window
    delay 0.5
    do script "mosquitto_sub -t \"Distancia\"" in front tab of the front window
    tell application "System Events" to keystroke "t" using command down
    delay 0.5
    do script "clear" in front tab of the front window
    delay 0.5
    do script "mosquitto_sub -t \"Temperatura\"" in front tab of the front window
    tell application "System Events" to keystroke "t" using command down
    delay 0.5
    do script "clear" in front tab of the front window
    delay 0.5
    do script "mosquitto_sub -t \"Incidencia_Luz\"" in front tab of the front window
end tell'

