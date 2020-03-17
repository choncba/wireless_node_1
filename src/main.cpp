/*
  NodoCochera MQTT para HomeAssistant
  Hardware:
          - ESP8266 12F
          - 2 entradas de Pulsadores
          - 2 Salidas a Triac
          - 1 entrada Sensor Temperatura DS18B20
*/

#include <Arduino.h>
#include <ESP8266WiFi.h>          // https://github.com/esp8266/Arduino
#include <PubSubClient.h>         // https://github.com/knolleary/pubsubclient
#include <ArduinoOTA.h>
#include <Ticker.h>
#include "config.h"

#if defined(DEBUG_TELNET)
WiFiServer  telnetServer(23);
WiFiClient  telnetClient;
#define     DEBUG_PRINT(x)    telnetClient.print(x)
#define     DEBUG_PRINTLN(x)  telnetClient.println(x)
#elif defined(DEBUG_SERIAL)
#define     DEBUG_PRINT(x)    Serial.print(x)
#define     DEBUG_PRINTLN(x)  Serial.println(x)
#else
#define     DEBUG_PRINT(x)
#define     DEBUG_PRINTLN(x)
#endif

WiFiClient    wifiClient;
PubSubClient  mqttClient(wifiClient);

void PublicarTodo(void);
void PublicarLuz(uint8_t);

#include <OneWire.h>
#include <DallasTemperature.h>
OneWire oneWire(OW_PIN);
DallasTemperature sensors(&oneWire);
Ticker read_time, publish_time, pub_all_time;

#define NUM_IO 2
const uint8_t Tecla[NUM_IO] = { PULSADOR1, PULSADOR2 };
enum Tecla_enum { tecla_patio = 0, tecla_galeria };
const char Tecla_txt[NUM_IO][14] = { "tecla_patio", "tecla_galeria" };
const uint8_t Luz[NUM_IO] = { SALIDA1, SALIDA2 };
enum Luz_enum { luz_patio = 0, luz_galeria };
const char Luz_txt [NUM_IO][12] = { "luz_patio", "luz_galeria" };

struct Status{
boolean Luz[NUM_IO] = {0,0};
uint8_t Tecla[NUM_IO] = {1,1};
float temp = 0;
}nodo;

#pragma region Debug TELNET
///////////////////////////////////////////////////////////////////////////
//   TELNET
///////////////////////////////////////////////////////////////////////////
/*
   Function called to handle Telnet clients
   https://www.youtube.com/watch?v=j9yW10OcahI
*/
#if defined(DEBUG_TELNET)
void handleTelnet(void) {
  if (telnetServer.hasClient()) {
    if (!telnetClient || !telnetClient.connected()) {
      if (telnetClient) {
        telnetClient.stop();
      }
      telnetClient = telnetServer.available();
    } else {
      telnetServer.available().stop();
    }
  }
}
#endif
#pragma endregion

#pragma region Funciones WiFi
///////////////////////////////////////////////////////////////////////////
//   WiFi
///////////////////////////////////////////////////////////////////////////

void handleWiFiEvent(WiFiEvent_t event) {
  switch (event) {
    case WIFI_EVENT_STAMODE_GOT_IP:
      DEBUG_PRINTLN(F("INFO: WiFi connected"));
      DEBUG_PRINT(F("INFO: IP address: "));
      DEBUG_PRINTLN(WiFi.localIP());
      break;
    case WIFI_EVENT_STAMODE_DISCONNECTED:
      DEBUG_PRINTLN(F("ERROR: WiFi losts connection"));
      /*
         TODO: Do something smarter than rebooting the device
      */
      delay(1000);
      WiFi.reconnect();
      //ESP.restart();
      break;
    default:
      DEBUG_PRINT(F("INFO: WiFi event: "));
      DEBUG_PRINTLN(event);
      break;
  }
}

void setupWiFi() {
  DEBUG_PRINT(F("INFO: WiFi connecting to: "));
  DEBUG_PRINTLN(WIFI_SSID);

  delay(10);

  WiFi.mode(WIFI_STA);
  WiFi.onEvent(handleWiFiEvent);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
#ifndef DHCP
    WiFi.config(NODE_IP, NODE_GW, NODE_MASK);
#endif

  randomSeed(micros());
}
#pragma endregion

#pragma region Funciones OTA
///////////////////////////////////////////////////////////////////////////
//   OTA
///////////////////////////////////////////////////////////////////////////
#if defined(OTA)
/*
   Function called to setup OTA updates
*/
void setupOTA() {
#if defined(OTA_HOSTNAME)
  ArduinoOTA.setHostname(OTA_HOSTNAME);
  DEBUG_PRINT(F("INFO: OTA hostname sets to: "));
  DEBUG_PRINTLN(OTA_HOSTNAME);
#endif

#if defined(OTA_PORT)
  ArduinoOTA.setPort(OTA_PORT);
  DEBUG_PRINT(F("INFO: OTA port sets to: "));
  DEBUG_PRINTLN(OTA_PORT);
#endif

#if defined(OTA_PASSWORD)
  ArduinoOTA.setPassword((const char *)OTA_PASSWORD);
  DEBUG_PRINT(F("INFO: OTA password sets to: "));
  DEBUG_PRINTLN(OTA_PASSWORD);
#endif

  ArduinoOTA.onStart([]() {
    DEBUG_PRINTLN(F("INFO: OTA starts"));
  });
  ArduinoOTA.onEnd([]() {
    DEBUG_PRINTLN(F("INFO: OTA ends"));
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    DEBUG_PRINT(F("INFO: OTA progresses: "));
    DEBUG_PRINT(progress / (total / 100));
    DEBUG_PRINTLN(F("%"));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    DEBUG_PRINT(F("ERROR: OTA error: "));
    DEBUG_PRINTLN(error);
    if (error == OTA_AUTH_ERROR)
      DEBUG_PRINTLN(F("ERROR: OTA auth failed"));
    else if (error == OTA_BEGIN_ERROR)
      DEBUG_PRINTLN(F("ERROR: OTA begin failed"));
    else if (error == OTA_CONNECT_ERROR)
      DEBUG_PRINTLN(F("ERROR: OTA connect failed"));
    else if (error == OTA_RECEIVE_ERROR)
      DEBUG_PRINTLN(F("ERROR: OTA receive failed"));
    else if (error == OTA_END_ERROR)
      DEBUG_PRINTLN(F("ERROR: OTA end failed"));
  });
  ArduinoOTA.begin();
}

/*
   Function called to handle OTA updates
*/
void handleOTA() {
  ArduinoOTA.handle();
}
#endif
#pragma endregion

#pragma region Funciones MQTT
///////////////////////////////////////////////////////////////////////////
//   MQTT
///////////////////////////////////////////////////////////////////////////
String StringTopic = "";
char CharTopic[100];
String StringData = "";
char CharData[10];

volatile unsigned long lastMQTTConnection = TIMEOUT;
/*
   Function called when a MQTT message has arrived
   @param p_topic   The topic of the MQTT message
   @param p_payload The payload of the MQTT message
   @param p_length  The length of the payload
*/
void handleMQTTMessage(char* p_topic, byte* p_payload, unsigned int p_length) {
  // concatenates the payload into a string
  uint8_t i = 0;
  String topic = String(p_topic);
  // for (i = 0; i < p_length; i++) {
  //   topic.concat((char)p_topic[i]);
  // }

  String payload;
  for (i = 0; i < p_length; i++) {
    payload.concat((char)p_payload[i]);
  }
  
  DEBUG_PRINTLN(F("INFO: New MQTT message received"));
  DEBUG_PRINT(F("INFO: MQTT topic: "));
  DEBUG_PRINTLN(topic);
  DEBUG_PRINT(F("INFO: MQTT payload: "));
  DEBUG_PRINTLN(payload);
  
  boolean set_value = false;
  for(i=0;i<NUM_IO;i++)           // Controla las salidas
  {
    if (topic.indexOf(Luz_txt[i])>0)
    {
      DEBUG_PRINT(F("FOUND: "));
      DEBUG_PRINTLN(Luz_txt[i]);
      if(payload.equals(ON))
      {
        set_value = true;
        DEBUG_PRINTLN(F("PAYLOAD: ON"));
      }  
      if(payload.equals(OFF))
      {
        set_value = false;
        DEBUG_PRINTLN(F("PAYLOAD: OFF"));
      } 
      
      if(set_value != nodo.Luz[i])
      {
        nodo.Luz[i] = set_value;
        digitalWrite(Luz[i], nodo.Luz[i]);
        DEBUG_PRINT(Luz_txt[i]);
        DEBUG_PRINT(F(" TURNED: "));
        DEBUG_PRINTLN((nodo.Luz[i])?"ON":"OFF");
      }
      PublicarLuz(i);
    }
  }
  
}

/*
  Function called to subscribe to a MQTT topic
*/
void subscribeToMQTT(char* p_topic) {
  if (mqttClient.subscribe(p_topic)) {
    DEBUG_PRINT(F("INFO: Sending the MQTT subscribe succeeded for topic: "));
    DEBUG_PRINTLN(p_topic);
  } else {
    DEBUG_PRINT(F("ERROR: Sending the MQTT subscribe failed for topic: "));
    DEBUG_PRINTLN(p_topic);
  }
}

/*
  Function called to publish to a MQTT topic with the given payload
*/
void publishToMQTT(char* p_topic, char* p_payload) {
  if (mqttClient.publish(p_topic, p_payload, RETAIN)) {
    DEBUG_PRINT(F("INFO: MQTT message published successfully, topic: "));
    DEBUG_PRINT(p_topic);
    DEBUG_PRINT(F(", payload: "));
    DEBUG_PRINTLN(p_payload);
  } else {
    DEBUG_PRINTLN(F("ERROR: MQTT message not published, either connection lost, or message too large. Topic: "));
    DEBUG_PRINT(p_topic);
    DEBUG_PRINT(F(" , payload: "));
    DEBUG_PRINTLN(p_payload);
  }
}

/*
  Function called to connect/reconnect to the MQTT broker
*/
void connectToMQTT()
{
  if (!mqttClient.connected())
  {
    if (lastMQTTConnection + TIMEOUT < millis())
    {
      StringTopic = String(BASE_TOPIC) + String(STATUS_TOPIC);
      StringTopic.toCharArray(CharTopic, sizeof(CharTopic));
      if (mqttClient.connect(MQTT_CLIENT_ID, MQTT_USERNAME, MQTT_PASSWORD, CharTopic, QoS, RETAIN, MQTT_DISCONNECTED_STATUS))
      {
        DEBUG_PRINTLN(F("INFO: The client is successfully connected to the MQTT broker"));
        publishToMQTT(CharTopic, MQTT_CONNECTED_STATUS);

        PublicarTodo(); // Publica los valores actuales a Home Assistant

        for(int i=0; i<NUM_IO; i++) // Me suscribo a los topics para encender luces
        {
          StringTopic = String(BASE_TOPIC) + "/" + String(Luz_txt[i]) + String(SET_TOPIC);
          StringTopic.toCharArray(CharTopic, sizeof(CharTopic));
          subscribeToMQTT(CharTopic);
        }

      } 
      else
      {
        DEBUG_PRINTLN(F("ERROR: The connection to the MQTT broker failed"));
        DEBUG_PRINT(F("INFO: MQTT username: "));
        DEBUG_PRINTLN(MQTT_USERNAME);
        DEBUG_PRINT(F("INFO: MQTT password: "));
        DEBUG_PRINTLN(MQTT_PASSWORD);
        DEBUG_PRINT(F("INFO: MQTT broker: "));
        DEBUG_PRINTLN(MQTT_SERVER);
      }
      lastMQTTConnection = millis();
    }
  }
}
#pragma endregion

#pragma region Publicaciones MQTT
void PublicarTecla(uint8_t i)
{
  StringTopic = String(BASE_TOPIC) + "/" + String(Tecla_txt[i]) + String(STATUS_TOPIC);
  StringTopic.toCharArray(CharTopic, sizeof(CharTopic));
  StringData = String((nodo.Tecla[i])?ON:OFF);
  StringData.toCharArray(CharData, sizeof(CharData));
  publishToMQTT(CharTopic, CharData);
}

void PublicarLuz(uint8_t i)
{
  StringTopic = String(BASE_TOPIC) + "/" + String(Luz_txt[i]) + String(STATUS_TOPIC);
  StringTopic.toCharArray(CharTopic, sizeof(CharTopic));
  StringData = String((nodo.Luz[i])?ON:OFF);
  StringData.toCharArray(CharData, sizeof(CharData));
  publishToMQTT(CharTopic, CharData);
}

void PublicarTemp()
{
  StringTopic = String(BASE_TOPIC) + String(TEMPERATURA_TOPIC);
  StringTopic.toCharArray(CharTopic, sizeof(CharTopic));
  StringData = String(nodo.temp);
  StringData.toCharArray(CharData, sizeof(CharData));
  publishToMQTT(CharTopic, CharData);
}

void PublicarWiFi()
{
  StringTopic = String(BASE_TOPIC) + String(WIFI_TOPIC) + "/ssid";
  StringTopic.toCharArray(CharTopic, sizeof(CharTopic));
  StringData = String(WiFi.SSID());
  StringData.toCharArray(CharData, sizeof(CharData));
  publishToMQTT(CharTopic, CharData);

  StringTopic = String(BASE_TOPIC) + String(WIFI_TOPIC) + "/ch";
  StringTopic.toCharArray(CharTopic, sizeof(CharTopic));
  StringData = String(WiFi.channel());
  StringData.toCharArray(CharData, sizeof(CharData));
  publishToMQTT(CharTopic, CharData);

  StringTopic = String(BASE_TOPIC) + String(WIFI_TOPIC) + "/rssi";
  StringTopic.toCharArray(CharTopic, sizeof(CharTopic));
  StringData = String(WiFi.RSSI());
  StringData.toCharArray(CharData, sizeof(CharData));
  publishToMQTT(CharTopic, CharData);
}

void PublicarTodo()
{
  for(int i = 0; i<NUM_IO; i++)
  {
    PublicarTecla(i);
    PublicarLuz(i);
  }

  PublicarTemp();

  PublicarWiFi();
}
#pragma endregion

#pragma region Lectura de Teclas y sensor

void CheckTeclas(){
  static uint8_t RoundCheck[NUM_IO] = { 0,0 };
  const uint8_t RoundCheckThreshole = 8;

  for (uint8_t cnt = 0; cnt < NUM_IO; cnt++)
  {
    uint8_t curStatus = digitalRead(Tecla[cnt]);
    if (nodo.Tecla[cnt] != curStatus)
    {
      delay(5);
      curStatus = digitalRead(Tecla[cnt]);
      if (nodo.Tecla[cnt] != curStatus)
      {
        RoundCheck[cnt]++;
      }
      else  RoundCheck[cnt] = 0;

      if (RoundCheck[cnt] >= RoundCheckThreshole)
      {
        if(nodo.Tecla[cnt]&!curStatus) // Si la tecla pasa de 1 a 0
        {
          nodo.Luz[cnt]^=1;            // Invierto la salida 
          digitalWrite(Luz[cnt], nodo.Luz[cnt]);  // Activo/Desactivo Salida
          PublicarLuz(cnt);
        }
        nodo.Tecla[cnt] = curStatus;
        RoundCheck[cnt] = 0;
        
        PublicarTecla(cnt);
      }
    }
  }
}

void ReadTemp()
{
  sensors.requestTemperatures();
  float int_temp = sensors.getTempCByIndex(0);
  if(!isnan(int_temp))  nodo.temp = int_temp;
}

#pragma endregion

///////////////////////////////////////////////////////////////////////////
//  SETUP() AND LOOP()
///////////////////////////////////////////////////////////////////////////

void setup()
{
  pinMode(PULSADOR1, INPUT);
  pinMode(PULSADOR2, INPUT);
  pinMode(SALIDA1, OUTPUT);
  pinMode(SALIDA2, OUTPUT);
#if defined(DEBUG_SERIAL)
  Serial.begin(115200);
#elif defined(DEBUG_TELNET)
  telnetServer.begin();
  telnetServer.setNoDelay(true);
#endif

  setupWiFi();

  mqttClient.setServer(MQTT_SERVER, MQTT_SERVER_PORT);
  mqttClient.setCallback(handleMQTTMessage);

  connectToMQTT();

#if defined(OTA)
  setupOTA();
#endif

  digitalWrite(SALIDA1, LOW);
  digitalWrite(SALIDA2, LOW);
  sensors.begin();
  read_time.attach(60, ReadTemp);
  publish_time.attach(600, PublicarTemp);
  pub_all_time.attach(1800, PublicarTodo);
  
  ReadTemp();
  PublicarTodo();
}

void loop()
{
#if defined(DEBUG_TELNET)
  handleTelnet();
  yield();
#endif

#if defined(OTA)
  handleOTA();
  yield();
#endif

  connectToMQTT();
  mqttClient.loop();
  yield();

  CheckTeclas();
  yield();
}