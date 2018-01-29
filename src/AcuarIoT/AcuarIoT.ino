// Librerías WiFiManager
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <WiFiManager.h>

// Librería DHT
#include <DHT.h>

// MQTT
#include <PubSubClient.h>

// Media
#include "RunningAverage.h"

/*
   Debug
*/
// Descomentar para ver mensajes por el monitor serie
#define ACUARIO_DEBUG

/*
   Configuración DHT11
*/
// Definimos el pin digital donde se conecta el sensor
#define DHTPIN D5
// Dependiendo del tipo de sensor
#define DHTTYPE DHT11
// Inicializamos el sensor DHT11
DHT dht(DHTPIN, DHTTYPE);

/*
   Configuración cliente Firebase
*/
// Cliente WiFi
WiFiClient clienteEsp;

/*
   Configuración tiempos
*/
unsigned long tiempoActual;
int tiempoActualizacion = 15000;

/*
   Configuración MQTT
*/
PubSubClient mqttCliente(clienteEsp);
const char* mqttServidor = "192.168.0.155";
const int mqttPuerto = 1883;
const char mqttTopicAcciones[] = "casa/acuario/acciones";
const char mqttTopicTemperaturaExt[] = "casa/servidor/tempext";
const char mqttTopicHumedadExt[] = "casa/servidor/humext";
const char mqttTopicTemperaturaAgua[] = "casa/servidor/tempagua";
const char mqttTopicPhAgua[] = "casa/servidor/phagua";

/*
   Calculo de la media
*/

RunningAverage mediaTemperatura(50);
RunningAverage mediaHumedad(50);

/*
   Definición: obtenerTempDHT11

   Propósito: obtener la temperatura en grados Celsius

   Parámetros: ninguno

   Return: boolean        Devuelve si ha habido algún error
*/
boolean obtenerTempDHT11() {
  // Leemos la temperatura en grados centígrados (por defecto)
  float temperatura = dht.readTemperature();

#ifdef ACUARIO_DEBUG
    Serial.print("[DHT11] Temperatura: ");
    Serial.print(temperatura);
    Serial.println(" ºC");
#endif

  // Comprobamos si ha habido algún error en la lectura
  if (isnan(temperatura)) {
#ifdef ACUARIO_DEBUG
    Serial.println("[DHT11] Error obteniendo la temperatura del sensor DHT11");
#endif
    return false;
  }

  // Calculo de la media
  mediaTemperatura.addValue(temperatura);

  return true;
}

/*
   Definición: obtenerHumDHT11

   Propósito: obtener la humedad en tanto por ciento

   Parámetros: ninguno

   Return: boolean        Devuelve si ha habido algún error
*/
float obtenerHumDHT11() {
  // Leemos la humedad relativa
  float humedad = dht.readHumidity();

#ifdef ACUARIO_DEBUG
    Serial.print("[DHT11] Humedad: ");
    Serial.print(humedad);
    Serial.println(" %");
#endif

  // Comprobamos si ha habido algún error en la lectura
  if (isnan(humedad)) {
#ifdef ACUARIO_DEBUG
    Serial.println("[DHT11] Error obteniendo la humedad del sensor DHT11");
#endif
    return -100;
  }

  // Calculo de la media
  mediaHumedad.addValue(humedad);

  return humedad;
}

/*
   Definición: obtenerIndiceDHT11

   Propósito: obtener el índice de calor

   Parámetros:
   float temperatura    Temperatura en º Celsius
   float humedad        Humedad en %

   Return: float        Indice de calor
*/
float obtenerIndiceDHT11() {
  // Calcular el índice de calor en grados centígrados
  float indiceCalor = dht.computeHeatIndex(mediaTemperatura.getAverage(), mediaHumedad.getAverage(), false);

  return indiceCalor;
}

/*
   Definición: mqttCallback

   Propósito: esta función se ejecuta cada vez que recibe un mensaje MQTT

   Parámetros:
   char* topic            Topic por el que se ha recibido el mensaje
   byte* mensaje          Mensaje recibido
   unsigned int longitud  Longitud del mensaje recibido

   Return: void           No devuelve nada
*/
void mqttCallback (char* topic, byte* mensaje, unsigned int longitud) {
#ifdef ACUARIO_DEBUG
  Serial.print("[MQTT] Mensaje recibido [");
  Serial.print(topic);
  Serial.print("]: ");
  for (int i = 0; i < longitud; i++) {
    Serial.print((char)mensaje[i]);
  }
  Serial.println();
#endif
}

/*
   Definición:  mqttReconectar

   Propósito:   volver a conectar con el broker MQTT

   Parámetros:

   Return: void           No devuelve nada
*/
void mqttReconectar() {
  // Repetimos hasta que se conecte
  while (!mqttCliente.connected()) {
#ifdef ACUARIO_DEBUG
    Serial.println("[MQTT] Esperando conexión con el broker MQTT...");
#endif
    // Intentando conectar
    if (mqttCliente.connect("ACUARIO")) {
#ifdef ACUARIO_DEBUG
      Serial.println("[MQTT] Conectado al broker MQTT");
      // Subscripción al topic de acciones
      mqttCliente.subscribe(mqttTopicAcciones);
#endif
    } else {
#ifdef ACUARIO_DEBUG
      Serial.print("[MQTT] Fallo al conectar MQTT, rc=");
      Serial.print(mqttCliente.state());
      Serial.println(" probando de nuevo en 5 segundos");
#endif
      // Esperamos 5 segundos para volver a conectar
      delay(5000);
    }
  }
}

/*
   Definición:  mqttPublicarTemperaturaExt

   Propósito:   publica la temperatura exterior en el broker MQTT

   Parámetros:

   Return: void           No devuelve nada
*/
void mqttPublicarTemperatura() {
  char msg[32];
  snprintf(msg, 32, "%2.1f", mediaTemperatura.getAverage());
  // Envío del mensaje al topic
  mqttCliente.publish(mqttTopicTemperaturaExt, msg);
#ifdef ACUARIO_DEBUG
  Serial.print("[MQTT] Publicando mensaje ");
  Serial.print(msg);
  Serial.print(" en el topic [");
  Serial.print(mqttTopicTemperaturaExt);
  Serial.println("]");
#endif
}

/*
   Definición:  mqttPublicarHumedadExt

   Propósito:   publica la humedad exterior en el broker MQTT

   Parámetros:

   Return: void           No devuelve nada
*/
void mqttPublicarHumedadExt() {
  char msg[32];
  snprintf(msg, 32, "%2.1f", mediaHumedad.getAverage());
  // Envío del mensaje al topic
  mqttCliente.publish(mqttTopicHumedadExt, msg);
#ifdef ACUARIO_DEBUG
  Serial.print("[MQTT] Publicando mensaje ");
  Serial.print(msg);
  Serial.print(" en el topic [");
  Serial.print(mqttTopicHumedadExt);
  Serial.println("]");
#endif
}

void setup() {
  // Inicializamos comunicación serie
  Serial.begin(115200);

  // Comenzamos el sensor DHT
  dht.begin();

  // Instancia a la clase WiFiManager
  WiFiManager wifiManager;

  // Configuramos el punto de acceso. Puedes poner el nombre que quieras
  wifiManager.autoConnect("ACUARIO-WIFI");

  // Establecemos tiempo
  tiempoActual = 0;

  // Configuración broker MQTT
  mqttCliente.setServer(mqttServidor, mqttPuerto);
  mqttCliente.setCallback(mqttCallback);

  // Limpiamos las medias
  mediaTemperatura.clear();
  mediaHumedad.clear();
}

void loop() {
  // Conexión con MQTT
  if (!mqttCliente.connected()) {
    // Volvemos a conectar
    mqttReconectar();
  }

  // Procesamos los mensajes entrantes
  mqttCliente.loop();

  // Sólo si ha pasado el tiempo actualizamos
  if (millis() - tiempoActual >= tiempoActualizacion ||
      tiempoActual == 0)
  {
    // Establecemos tiempo
    tiempoActual = millis();

    // Comprobación publicación MQTT
    boolean mqttPublicar=true;

    // Obtenemos la temperatura
    mqttPublicar = mqttPublicar && obtenerTempDHT11();
    // Obtenemos la humedad
    mqttPublicar = mqttPublicar && obtenerHumDHT11();

    float indiceCalorDHT11;
    if (mqttPublicar) {
      // Obtenemos el índice de calor
      indiceCalorDHT11 = obtenerIndiceDHT11();
    }

    // Envío de datos al broker MQTT
    if (mqttPublicar) {
      // Temperatura
      mqttPublicarTemperatura();
      // Humedad
      mqttPublicarHumedadExt();
    }
  }
}

