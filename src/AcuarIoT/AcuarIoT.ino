// Librerías WiFiManager
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>

// Librería DHT
#include <DHT.h>

// Librería Json
#include <ArduinoJson.h>

// NTP Network Time Protocol (sincronización fecha y hora)
#include <NTPClient.h>
#include <WiFiUdp.h>

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
WiFiClientSecure cliente;
// Host o url de Firebase
#define HOSTFIREBASE "acuario-arduino.firebaseio.com"

const byte tamanoBufferJson = 200;

/*
   Configuración NTPclient
*/
WiFiUDP ntpUDP;
// Por defecto utiliza el servidor 'time.nist.gov' y actualiza cada 60 segundos
NTPClient timeClient(ntpUDP);
unsigned long tiempoActual;
int tiempoActualizacion = 60000;

/*
   Definición: enviarDatosFirebase

   Propósito: envía los datos a Firebase

   Parámetros:
   float temperaturaDHT11 Temperatura en grados Celsius del DHT11
   float humedadDHT11     Humedad en tanto por ciento del DHT11
   float indiceCarloDHT11 Indice de calor grados Celsius del DHT11

   Return: boolean        Si ha conseguido enviar los datos
*/
boolean enviarDatosFirebase(float temperaturaDHT11, float humedadDHT11, float indiceCarloDHT11) {
  // Reservamos los bytes del JSON
  StaticJsonBuffer<tamanoBufferJson> jsonBuffer;

  // Creamos un objeto JSON
  JsonObject& mensajeJson = jsonBuffer.createObject();

  // Agregamos valores al JSON
  mensajeJson["temperaturaext"] = temperaturaDHT11;
  mensajeJson["humedadext"] = humedadDHT11;
  mensajeJson["indicecalorext"] = indiceCarloDHT11;

  // Lo convertimos en una cadena de texto
  String mensajeHttpJson = "";
  mensajeJson.prettyPrintTo(mensajeHttpJson);
  mensajeHttpJson += "\r\n";

  // Obtenemos la fecha timestamp
  timeClient.update();
  unsigned long fechaTimestamp = timeClient.getEpochTime();

  // Cerramos cualquier conexión antes de enviar una nueva petición
  cliente.stop();
  cliente.flush();

  // Enviamos una petición por SSL
  if (cliente.connect(HOSTFIREBASE, 443)) {
    // Petición PUT JSON
    cliente.print("PUT /");
    cliente.print(fechaTimestamp);
    cliente.println(".json HTTP/1.1");
    cliente.print("Host:");
    cliente.println(HOSTFIREBASE);
    cliente.println("Content-Type: application/json");
    cliente.println("Content-Length: " + String(mensajeHttpJson.length()));
    cliente.println("");
    cliente.println(mensajeHttpJson);
#ifdef ACUARIO_DEBUG
    Serial.println("******* HTTP REQUEST FIREBASE *******");
    Serial.print("PUT /");
    Serial.print(fechaTimestamp);
    Serial.println(".json HTTP/1.1");
    Serial.print("Host: ");
    Serial.println(HOSTFIREBASE);
    Serial.println("Content-Type: application/json");
    Serial.println("Cache-Control: no-cache");
    Serial.println("Content-Length: " + String(mensajeHttpJson.length()));
    Serial.println("");
    Serial.println(mensajeHttpJson);

    // Leemos respuesta del servidor
    Serial.println("******* HTTP RESPONSE FIREBASE *******");
    while (cliente.connected()) {
      String line = cliente.readStringUntil('\n');
      Serial.println(line);
      if (line == "\r") {
        break;
      }
    }
#endif
    cliente.flush();
    cliente.stop();
  } else {
    // Si no podemos conectar
    cliente.flush();
    cliente.stop();
#ifdef ACUARIO_DEBUG
    Serial.println("Error llamada a Firebase");
#endif
    return false;
  }

  return true;
}

/*
   Definición: obtenerTempDHT11

   Propósito: obtener la temperatura en grados Celsius

   Parámetros: ninguno

   Return: float        Temperatura en grados Celsius
*/
float obtenerTempDHT11() {
  // Leemos la temperatura en grados centígrados (por defecto)
  float temperatura = dht.readTemperature();

  // Comprobamos si ha habido algún error en la lectura
  if (isnan(temperatura)) {
#ifdef ACUARIO_DEBUG
    Serial.println("Error obteniendo la temperatura del sensor DHT11");
#endif
    return -100;
  }

  return temperatura;
}

/*
   Definición: obtenerHumDHT11

   Propósito: obtener la humedad en tanto por ciento

   Parámetros: ninguno

   Return: float        Humedad en tanto por ciento
*/
float obtenerHumDHT11() {
  // Leemos la humedad relativa
  float humedad = dht.readHumidity();

  // Comprobamos si ha habido algún error en la lectura
  if (isnan(humedad)) {
#ifdef ACUARIO_DEBUG
    Serial.println("Error obteniendo la humedad del sensor DHT11");
#endif
    return -100;
  }

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
float obtenerIndiceDHT11(float temperatura, float humedad) {
  // Calcular el índice de calor en grados centígrados
  float indiceCalor = dht.computeHeatIndex(temperatura, humedad, false);

  return indiceCalor;
}

void setup() {
  // Inicializamos comunicación serie
  Serial.begin(9600);

  // Comenzamos el sensor DHT
  dht.begin();

  // Instancia a la clase WiFiManager
  WiFiManager wifiManager;

  // Configuramos el punto de acceso. Puedes poner el nombre que quieras
  wifiManager.autoConnect("ACUARIO-WIFI");

  // Iniciamos el NTPClient
  timeClient.begin();

  // Establecemos tiempo
  tiempoActual = millis();
}

void loop() {
  // Sólo si ha pasado el tiempo actualizamos
  if (millis() - tiempoActual >= tiempoActualizacion)
  {
    // Establecemos tiempo
    tiempoActual = millis();

    // Obtenemos la temperatura
    float temperaturaDHT11 = obtenerTempDHT11();
    // Obtenemos la humedad
    float humedadDHT11 = obtenerHumDHT11();
    // Obtenemos el índice de calor
    float indiceCalorDHT11 = obtenerIndiceDHT11(temperaturaDHT11, humedadDHT11);

#ifdef ACUARIO_DEBUG
    Serial.println("******* DHT11 *******");
    Serial.print("Humedad: ");
    Serial.print(humedadDHT11);
    Serial.print(" %\t");
    Serial.print("Temperatura: ");
    Serial.print(temperaturaDHT11);
    Serial.print(" *C\t");
    Serial.print("Índice de calor: ");
    Serial.print(indiceCalorDHT11);
    Serial.println(" *C ");
#endif

    // Enviamos los datos a Firebase
    while (!enviarDatosFirebase(temperaturaDHT11, humedadDHT11, indiceCalorDHT11)) {
#ifdef ACUARIO_DEBUG
      Serial.println("Esperando 10 segundos");
#endif
      // Esperamos 10 segundos
      delay(10000);
    }
  }
}
