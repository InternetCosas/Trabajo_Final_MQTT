/*
 * 21KOhm -> 2 resistencias de 10K y una de 1K en serie
 * Pines usados: 5V, GND y A1
 */

#include <math.h>

const int Rc = 10000; //valor de la resistencia
const int Vcc = 5;
const int SensorPIN = A1;

float A = 1.11492089e-3;
float B = 2.372075385e-4;
float C = 6.954079529e-8;

float K = 2.5; //factor de disipacion en mW/C

void setup()
{
  Serial.begin(9600);
}

void loop() 
{
  float raw = analogRead(SensorPIN);
  float V =  raw / 1024 * Vcc;

  float R = (Rc * V ) / (Vcc - V);
  

  float logR  = log(R);
  float R_th = 1.0 / (A + B * logR + C * logR * logR * logR );

  float kelvin = R_th - V*V/(K * R)*1000;
  float celsius = kelvin - 273.15;

  Serial.print("T = ");
  Serial.print(celsius);
  Serial.print("C\n");
  delay(2500);
}