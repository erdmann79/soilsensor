#include "secrets.h"
#include "SPI.h"
#include "Adafruit_seesaw.h"
#include "ESP8266WiFi.h"
#include "ESP8266mDNS.h"
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"
#include "ESPBattery.h"

/************************* ESP Battery Diagnostic  ***************************/
ESPBattery battery = ESPBattery();

/************************* Adafruit_seesaw ***********************************/
Adafruit_seesaw seesaw;
#define CAPACITY_THRESHHOLD   550
bool waterpresence;

/***************************** Global State **********************************/

// Create an ESP8266 WiFiClient class to connect to the MQTT server.
//WiFiClient client;
// or... use WiFiFlientSecure for SSL , MQTT_USERNAME, MQTT_KEY
WiFiClient client;

// Setup the MQTT client class by passing in the WiFi client and MQTT server and login details.
Adafruit_MQTT_Client mqtt(&client, MQTT_SERVER, MQTT_SERVERPORT);


/****************************** Feeds ****************************************/
// Setup a feed called 'ultrasonic' for publishing.
// Notice MQTT paths for MQTT follow the form: <username>/feeds/<feedname>

Adafruit_MQTT_Publish tempread = Adafruit_MQTT_Publish(&mqtt, HOSTNAME "/Temperature");
Adafruit_MQTT_Publish waterread = Adafruit_MQTT_Publish(&mqtt, HOSTNAME "/WaterDetected");
Adafruit_MQTT_Publish voltread = Adafruit_MQTT_Publish(&mqtt, HOSTNAME "/BattVoltage");
Adafruit_MQTT_Publish percentread = Adafruit_MQTT_Publish(&mqtt, HOSTNAME "/BattPercent");
Adafruit_MQTT_Publish stateread = Adafruit_MQTT_Publish(&mqtt, HOSTNAME "/BattState");
Adafruit_MQTT_Publish levelread = Adafruit_MQTT_Publish(&mqtt, HOSTNAME "/BattLevel");
Adafruit_MQTT_Publish capread = Adafruit_MQTT_Publish(&mqtt, HOSTNAME "/Capacitive");

// Bug workaround for Arduino 1.6.6, it seems to need a function declaration
// for some reason (only affects ESP8266, likely an arduino-builder bug).
// void MQTT_connect();

/****************************** Functions ************************************/
void MQTT_connect() {
  int8_t ret;
  // Stop if already connected.
  if (mqtt.connected()) {
    return;
  }
  Serial.print("Connecting to MQTT... ");
  uint8_t retries = 3;
  while ((ret = mqtt.connect()) != 0) { // connect will return 0 for connected
       Serial.println(mqtt.connectErrorString(ret));
       Serial.println("Retrying MQTT connection in 5 seconds...");
       mqtt.disconnect();
       delay(5000);  // wait 5 seconds
       retries--;
       if (retries == 0) {
         // basically die and wait for WDT to reset me
         while (1);
       }
  }
  Serial.println("MQTT Connected!");
}

/******************************/
// function that will be called when the battery state changes
void stateHandler(ESPBattery& b) {
  int state = b.getState();
  int level = b.getLevel();
  float voltage = b.getVoltage();
  int percentage = b.getPercentage();

  Serial.print("\nCurrent Voltage: ");
  Serial.println(voltage);
  Serial.print("Current Percentage: ");
  Serial.println(percentage);
  Serial.print("Current state: ");
  Serial.println(b.stateToString(state));
  Serial.print("Current level: ");
  Serial.println(level);
}

/******************************/
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println(); Serial.println();

  // initalize battery
  Serial.println(F("Init ESPBattery Soil Moisture Sensor"));
  Serial.println(F("Arduino Library to calculate the ESP8266 (Feather Huzzah) LiPo battery level"));
  
  // attach handler function
  battery.setLevelChangedHandler(stateHandler);
  // call the handler once to display the current status
  stateHandler(battery);
  Serial.println(); 


  // initalize seesaw
  Serial.println(F("Init Capacitive Soil Moisture Sensor"));  
  if (!seesaw.begin(0x36)) {
    Serial.println("ERROR! Sensor Not found");
    while(1) delay(1);
  } else {
    Serial.print("SEESAW Started! Version: "); Serial.println(seesaw.getVersion(), HEX);
  }
  Serial.println();

  //Set new hostname
  WiFi.mode(WIFI_STA);
  WiFi.hostname(HOSTNAME);
  MDNS.begin(HOSTNAME);

  // Connect to WiFi access point.
  Serial.print("Connecting to "); Serial.println(WLAN_SSID);
  WiFi.begin(WLAN_SSID, WLAN_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("WiFi connected");
  Serial.printf("Hostname: %s\n", WiFi.hostname().c_str());
  Serial.print("IP address: "); Serial.println(WiFi.localIP());
  Serial.println();
}


void loop() {
  battery.loop();
  float tempC = seesaw.getTemp();
  uint16_t capacity = seesaw.touchRead(0);
  
  int state = battery.getState();
  int level = battery.getLevel();
  float voltage = battery.getVoltage();
  int percentage = battery.getPercentage();
  String stateString = battery.stateToString(state);

  delay(15000);
  MQTT_connect();
  Serial.print("Battery Voltage: "); Serial.println(voltage);
  Serial.print("Battery Percentage: "); Serial.println(percentage);
  Serial.print("Battery State: "); Serial.println(stateString);
  Serial.print("Battery Level: "); Serial.println(level);
  Serial.print("Temperature: "); Serial.print(tempC); Serial.println("*C");
  Serial.print("Capacitive: "); Serial.println(capacity);
  waterpresence  = false;
  if( capacity > CAPACITY_THRESHHOLD ){
    Serial.println("Water Detected: True");
    waterpresence  = "true";
    } else {
    Serial.println("Water Detected: False");
    }

  Serial.print(F("Send to MQTT: "));
  if ( percentread.publish(percentage) \
    && capread.publish(capacity) \
    && voltread.publish(voltage) \
    && tempread.publish(tempC) \
    && levelread.publish(level) \
    && stateread.publish((char*) stateString.c_str()) \
    && waterread.publish(waterpresence)) {
    Serial.println(F("Successful"));
  } else {
    Serial.println(F("Failed"));
  }


  Serial.println();
}



