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

// DS18B20
#include <DallasTemperature.h>

// Pantalla OLED
#include <Wire.h>
#include "SSD1306.h"
#include "OLEDDisplayUi.h"

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
const char* mqttServidor = "192.168.1.100";
const int mqttPuerto = 1883;
const char mqttTopicLuz[] = "casa/acuario/luz";
const char mqttTopicTemperaturaExt[] = "casa/servidor/tempext";
const char mqttTopicHumedadExt[] = "casa/servidor/humext";
const char mqttTopicIndiceExt[] = "casa/servidor/indiceext";
const char mqttTopicTemperaturaAgua[] = "casa/servidor/tempagua";
const char mqttTopicPhAgua[] = "casa/servidor/phagua";

/*
   Calculo de la media
*/

RunningAverage mediaTemperatura(50);
RunningAverage mediaHumedad(50);
RunningAverage mediaTemperaturaAgua(5);

/*
   Cálculo temperatura agua
*/
// Pin donde se conecta el bus 1-Wire
const byte ds18b20Pin = D6;
// Instancia a las clases OneWire y DallasTemperature
OneWire ds18b20OneWireObjeto(ds18b20Pin);
DallasTemperature ds18b20Sensor(&ds18b20OneWireObjeto);

/*
   Relé
*/
const byte relePin = D7;
boolean releEstado = false;

/*
   Pantalla OLED
*/
SSD1306  display(0x3c, D3, D4);
OLEDDisplayUi ui     ( &display );

/*
   Sensor de Ph
*/
const byte phPin = A0;
float phOffset = 0.00;
RunningAverage mediaPh(40);
/*
  Pantallas a dibujar
*/
void drawFrame1(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);
  display->drawString(71 + x, 15 + y, "SENSACION TERMICA");

  display->setFont(ArialMT_Plain_24);
  display->drawString(70 + x, 34 + y, String(dht.computeHeatIndex(mediaTemperatura.getAverage(), mediaHumedad.getAverage(), false)));
}

void drawFrame2(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);
  display->drawString(71 + x, 15 + y, "TEMP AGUA");

  display->setFont(ArialMT_Plain_24);
  display->drawString(70 + x, 34 + y, String(mediaTemperaturaAgua.getAverage()));
}

void drawFrame3(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);
  display->drawString(71 + x, 15 + y, "PH AGUA");

  display->setFont(ArialMT_Plain_24);
  display->drawString(70 + x, 34 + y, String(obtenerPh()));
}

void drawFrame4(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);
  display->drawString(71 + x, 15 + y, "LUZ");

  display->setFont(ArialMT_Plain_24);
  display->drawString(70 + x, 34 + y, (releEstado?"ON":"OFF"));
}

void drawProgressBar(int fase) {
  int progress = (fase / 5) % 100;
  // draw the progress bar
  display.drawProgressBar(0, 32, 120, 10, progress);

  // draw the percentage as String
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.drawString(64, 15, String(progress) + "%");
}

// Array con las pantallas
FrameCallback frames[] = { drawFrame1, drawFrame2, drawFrame3, drawFrame4};
int frameCount = 4;

// Configuración de UI Oled
void uiConfigOled(){
  // The ESP is capable of rendering 60fps in 80Mhz mode
  // but that won't give you much time for anything else
  // run it in 160Mhz mode or just set it to 30 fps
  ui.setTargetFPS(60);

  // Customize the active and inactive symbol
  //ui.setActiveSymbol(activeSymbol);
  //ui.setInactiveSymbol(inactiveSymbol);

  // You can change this to
  // TOP, LEFT, BOTTOM, RIGHT
  ui.setIndicatorPosition(LEFT);

  // Defines where the first frame is located in the bar.
  ui.setIndicatorDirection(LEFT_RIGHT);

  // You can change the transition that is used
  // SLIDE_LEFT, SLIDE_RIGHT, SLIDE_UP, SLIDE_DOWN
  ui.setFrameAnimation(SLIDE_DOWN);

  // Add frames
  ui.setFrames(frames, frameCount);

  // Add overlays
  //ui.setOverlays(overlays, overlaysCount);

  // Initialising the UI will init the display too.
  ui.init();

  display.flipScreenVertically();
}

/*
   Definición: obtenerPh

   Propósito: obtiene el valor de pH

   Parámetros: ninguno

   Return:     float: valor de pH
*/
float obtenerPh(){
  // Lectura pin analógico
  int valorPinAnalogicoPh = analogRead(phPin);

  // Calculo de la media
  mediaPh.addValue(valorPinAnalogicoPh);

  // Cálculo del voltaje
  float voltajePh = mediaPh.getAverage()*5.0/1024;

#ifdef ACUARIO_DEBUG
  Serial.print("[PH] pH: ");
  Serial.println((voltajePh * 3.5) + phOffset);
#endif

  return (voltajePh * 3.5) + phOffset;
}

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
   Definición: obtenerTempDs18b20

   Propósito: obtener la temperatura en grados Celsius

   Parámetros: ninguno

   Return: void        No devuelve ningún valor
*/
boolean obtenerTempDs18b20() {

  // Obtención de temperatura DS18B20
  ds18b20Sensor.requestTemperatures();
  float temperatura = ds18b20Sensor.getTempCByIndex(0);

#ifdef ACUARIO_DEBUG
  Serial.print("[DS18B20] Temperatura: ");
  Serial.print(temperatura);
  Serial.println(" ºC");
#endif

  // Calculo de la media
  mediaTemperaturaAgua.addValue(temperatura);

  return true;
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
  Serial.print("--");
  Serial.print(mqttTopicLuz);
  Serial.print("]: ");
  for (int i = 0; i < longitud; i++) {
    Serial.print((char)mensaje[i]);
  }
  Serial.println();
#endif

  // Señal para recibir un mensaje
  digitalWrite(LED_BUILTIN, HIGH);
  delay(1000);
  digitalWrite(LED_BUILTIN, LOW);
  delay(1000);

  // Comprobamos el topic
  if (String(topic) == mqttTopicLuz) {
    // Sólo si tiene un carácter
    if (longitud == 1) {
      if (char(mensaje[0]) == '1') {
        // Encendemos la luz
        digitalWrite(relePin, HIGH);
        releEstado = true;
      } else {
        // Apagamos la luz
        digitalWrite(relePin, LOW);
        releEstado = false;
      }
    }
  }

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
      mqttCliente.subscribe(mqttTopicLuz);
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

/*
   Definición:  mqttPublicarIndiceExt

   Propósito:   publica el índice de calor exterior en el broker MQTT

   Parámetros:
   float        Indice de calor en grados Celsius

   Return: void No devuelve nada
*/
void mqttPublicarIndiceExt(float indiceCalor) {
  char msg[32];
  snprintf(msg, 32, "%2.1f", indiceCalor);
  // Envío del mensaje al topic
  mqttCliente.publish(mqttTopicIndiceExt, msg);
#ifdef ACUARIO_DEBUG
  Serial.print("[MQTT] Publicando mensaje ");
  Serial.print(msg);
  Serial.print(" en el topic [");
  Serial.print(mqttTopicIndiceExt);
  Serial.println("]");
#endif
}

/*
   Definición:  mqttPublicarTemperaturaAgua

   Propósito:   publica el índice de calor exterior en el broker MQTT

   Parámetros:

   Return: void No devuelve nada
*/
void mqttPublicarTemperaturaAgua() {
  char msg[32];
  snprintf(msg, 32, "%2.1f", mediaTemperaturaAgua.getAverage());
  // Envío del mensaje al topic
  mqttCliente.publish(mqttTopicTemperaturaAgua, msg);
#ifdef ACUARIO_DEBUG
  Serial.print("[MQTT] Publicando mensaje ");
  Serial.print(msg);
  Serial.print(" en el topic [");
  Serial.print(mqttTopicTemperaturaAgua);
  Serial.println("]");
#endif
}

/*
   Definición:  mqttPublicarPh

   Propósito:   publica el pH en el broker MQTT

   Parámetros: ninguno

   Return:     void No devuelve nada
*/
void mqttPublicarPh(){
  char msg[32];
  snprintf(msg, 32, "%2.1f", obtenerPh());
  // Envío del mensaje al topic
  mqttCliente.publish(mqttTopicPhAgua, msg);
#ifdef ACUARIO_DEBUG
  Serial.print("[MQTT] Publicando mensaje ");
  Serial.print(msg);
  Serial.print(" en el topic [");
  Serial.print(mqttTopicPhAgua);
  Serial.println("]");
#endif
}

void setup() {
#ifdef ACUARIO_DEBUG
  // Inicializamos comunicación serie
  Serial.begin(115200);
#endif

  // Iniciamos la pantalla OLED
  display.init();
  display.setFont(ArialMT_Plain_10);

  // Limpiamos la pantalla
  display.clear();
  drawProgressBar(1);
  display.display();

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  pinMode(relePin, OUTPUT);
  digitalWrite(relePin, releEstado);

  // Comenzamos el sensor DHT
  dht.begin();

  // Instancia a la clase WiFiManager
  WiFiManager wifiManager;

  drawProgressBar(2);
  display.display();

  // Configuramos el punto de acceso. Puedes poner el nombre que quieras
  wifiManager.autoConnect("ACUARIO-WIFI");

  // Establecemos tiempo
  tiempoActual = 0;

  drawProgressBar(3);
  display.display();

  // Configuración broker MQTT
  mqttCliente.setServer(mqttServidor, mqttPuerto);
  mqttCliente.setCallback(mqttCallback);

  // Limpiamos las medias
  mediaTemperatura.clear();
  mediaHumedad.clear();

  drawProgressBar(4);
  display.display();

  // Iniciamos el bus 1-Wire
  ds18b20Sensor.begin();

  drawProgressBar(5);
  display.display();

  // Configuración OLED
  uiConfigOled();

  // Apagamos LED
  digitalWrite(LED_BUILTIN, LOW);
}

void loop() {
  ui.update();

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
    boolean mqttPublicar = true;

    // Obtenemos la temperatura
    mqttPublicar = mqttPublicar && obtenerTempDHT11();
    // Obtenemos la humedad
    mqttPublicar = mqttPublicar && obtenerHumDHT11();

    float indiceCalorDHT11;
    if (mqttPublicar) {
      // Obtenemos el índice de calor
      indiceCalorDHT11 = obtenerIndiceDHT11();
    }

    // Obtenemos temperatura agua
    obtenerTempDs18b20();

    // Envío de datos al broker MQTT
    if (mqttPublicar) {
      // Temperatura ambiente
      mqttPublicarTemperatura();
      // Humedad ambiente
      mqttPublicarHumedadExt();
      // Indice Calor
      mqttPublicarIndiceExt(indiceCalorDHT11);
      // Publicar temperatura agua
      mqttPublicarTemperaturaAgua();
      // Publicar pH
      mqttPublicarPh();
    }
  }
}

