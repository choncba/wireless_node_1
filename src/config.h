///////////////////////////////////////////////////////////////////////////
//   PINS
///////////////////////////////////////////////////////////////////////////
#define PULSADOR1   D1  // GPIO5 - Tecla Patio    
#define PULSADOR2   D2  // GPIO4 - Tecla Galeria  
#define SALIDA1     D6  // GPIO12 - Luz Patio  
#define SALIDA2     D7  // GPIO13 - Luz Galeria    
#define OW_PIN      D4    

///////////////////////////////////////////////////////////////////////////
//   WiFi
///////////////////////////////////////////////////////////////////////////
#define WIFI_SSID       "*****"
#define WIFI_PASSWORD   "*****"
//#define DHCP            // uncomment for use DHCP
#ifndef DHCP
IPAddress NODE_IP(192,168,1,52);
IPAddress NODE_GW(192,168,1,1);
IPAddress NODE_MASK(255,255,255,0);
#endif
///////////////////////////////////////////////////////////////////////////
//   MQTT
///////////////////////////////////////////////////////////////////////////
#define MQTT_CLIENT_ID    "NodoGaleria"
#define MQTT_USERNAME     "********"       // BRIX
#define MQTT_PASSWORD     "********"
#define MQTT_SERVER       "192.168.1.3" // BRIX
#define MQTT_SERVER_PORT  1883

#define BASE_TOPIC "/" MQTT_CLIENT_ID
#define SET_TOPIC "/set"
#define STATUS_TOPIC "/status"
#define TEMPERATURA_TOPIC "/temperatura"
#define WIFI_TOPIC "/wifi"
#define MQTT_CONNECTED_STATUS "online"
#define MQTT_DISCONNECTED_STATUS "offline"

#define ON   "1"
#define OFF  "0"

#define TIMEOUT 5000

#define RETAIN true
#define QoS     0

///////////////////////////////////////////////////////////////////////////
//   DEBUG
///////////////////////////////////////////////////////////////////////////
#define DEBUG_TELNET
//#define DEBUG_SERIAL

///////////////////////////////////////////////////////////////////////////
//   OTA
///////////////////////////////////////////////////////////////////////////
#define OTA
#define OTA_HOSTNAME  MQTT_CLIENT_ID  // hostname esp8266-[ChipID] by default
//#define OTA_PASSWORD  "password"  // no password by default
//#define OTA_PORT      8266        // port 8266 by default
