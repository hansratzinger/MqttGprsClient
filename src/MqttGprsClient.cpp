/**************************************************************
 * MqttGprsClient.cpp
 * uses the GPRS module to connect to the internet and send data to an MQTT broker.
 * based on the TinyGSM and PubSubClient libraries
 * HR 2025-03-15  NK
 **************************************************************/
// branch getSerialFromGateway
#define TINY_GSM_MODEM_SIM7000

// Set serial for debug console (to the Serial Monitor, default speed 115200)
#define SerialMon Serial

// Set serial for AT commands (to the module)
// Use Hardware Serial on Mega, Leonardo, Micro
#define SerialAT Serial1

// Define the serial connection for UART
#define SerialUART Serial2

// See all AT commands, if wanted
// #define DUMP_AT_COMMANDS

// Define the serial console for debug prints, if needed
// #define TINY_GSM_DEBUG SerialMon

#define TINY_GSM_USE_GPRS true
#define TINY_GSM_USE_WIFI false

#define uS_TO_S_FACTOR 1000000ULL  // Conversion factor for micro seconds to seconds
// #define TIME_TO_SLEEP  10          // Time ESP32 will go to sleep (in seconds)

#define UART_BAUD  115200
#define AT_BAUD    115200
#define PIN_DTR        25
#define AT_PIN_TX      27
#define AT_PIN_RX      26
#define UART_RX_PIN    33 // Define RX pin for Serial1
#define UART_TX_PIN    32 // Define TX pin for Serial1
#define UART_RX_BUFFER_SIZE 1024 // Erhöhen Sie die Puffergröße
#define PWR_PIN        4

#define SD_MISO        2
#define SD_MOSI       15
#define SD_SCLK       14
#define SD_CS         13
#define LED_PIN       12

// set GSM PIN, if any
#define GSM_PIN ""

#include <Arduino.h>
#include <WString.h>
#include <TinyGsmClient.h>
#include <PubSubClient.h>
#include <Ticker.h>
#include <SPI.h>
#include <SD.h>
#include <queue>
#include <ArduinoJson.h>
#include "time.h"
// #include <iostream>

// Your GPRS credentials, if any
const char apn[]      = "hologram";
const char gprsUser[] = "";
const char gprsPass[] = "";

// MQTT details
// const char broker[] = "broker.hivemq.com";
const char broker[] = "test.mosquitto.org";
// const char broker[] = "194c32edd2ba406fb8ecdba22f6f7816.s1.eu.hivemq.cloud";
int port = 1883;

const char* topicLed       = "NickGhResearch/led";
const char* topicInit      = "NickGhResearch/init";
const char* topicLedStatus = "NickGhResearch/ledStatus";
const char* topicPosition = "NickGhResearch/Position"; // Topic for sensor data
const char* topicRpm1 = "NickGhResearch/Rpm1"; // Topic for sensor data
const char* topicRpm2 = "NickGhResearch/Rpm2"; // Topic for sensor data

const char clientID[] = "gruenthal99";
const char user[] = "towerstation2025";
const char password[] = "SkaYbXphGO4BTZxmleZx";

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 0;
const int   daylightOffset_sec = 3600;

unsigned long lastPublishTime = 0; // Variable to store the last publish time
const unsigned long publishInterval = 1000; // Interval to publish data (e.g., every 1 second)
const unsigned long dataCollectionInterval = 1000; // Interval to collect data (e.g., every 1 second)
unsigned long lastDataCollectionTime = 0; // Variable to store the last data collection time

#ifdef DUMP_AT_COMMANDS
#include <StreamDebugger.h>
StreamDebugger debugger(SerialAT, SerialMon);
TinyGsm        modem(debugger);
#else
TinyGsm        modem(SerialAT);
#endif
TinyGsmClient client(modem);
PubSubClient  mqtt(client);

Ticker tick;

struct tm timeinfo;

std::queue<String> dataQueue; // Queue to store the data

int ledStatus = LOW;
uint32_t lastReconnectAttempt = 0;
bool modemInitialized = false;

extern "C" {
    void lwip_hook_ip6_input() {
        // Dummy implementation to avoid linker error
    }
}

int getTopic(String payload) {
    std::string str = payload.c_str();
    std::string id1 = "\"id\":1,";
    std::string id2 = "\"id\":2,";
    std::string id5 = "\"id\":5,";
    
    // Suchen nach dem Substring id1
    if (str.find(id1) != std::string::npos) {
        return 1;
    }
    // Suchen nach dem Substring id2
    if (str.find(id2) != std::string::npos) {
        return 2;
    }    
    // Suchen nach dem Substring id5
    if (str.find(id5) != std::string::npos) {
        return 5;
    }
    // Wenn keiner der Substrings gefunden wird, gib 0 zurück
    return 0;
}
void printLocalTime(){
    struct tm timeinfo;
    if(!getLocalTime(&timeinfo)){
      SerialMon.println("Failed to obtain time");
      return;
    }
    SerialMon.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
    SerialMon.print("Day of week: ");
    SerialMon.println(&timeinfo, "%A");
    SerialMon.print("Month: ");
    SerialMon.println(&timeinfo, "%B");
    SerialMon.print("Day of Month: ");
    SerialMon.println(&timeinfo, "%d");
    SerialMon.print("Year: ");
    SerialMon.println(&timeinfo, "%Y");
    SerialMon.print("Hour: ");
    SerialMon.println(&timeinfo, "%H");
    SerialMon.print("Hour (12 hour format): ");
    SerialMon.println(&timeinfo, "%I");
    SerialMon.print("Minute: ");
    SerialMon.println(&timeinfo, "%M");
    SerialMon.print("Second: ");
    SerialMon.println(&timeinfo, "%S");
  
    SerialMon.println("Time variables");
    char timeHour[3];
    strftime(timeHour,3, "%H", &timeinfo);
    SerialMon.println(timeHour);
    char timeWeekDay[10];
    strftime(timeWeekDay,10, "%A", &timeinfo);
    SerialMon.println(timeWeekDay);
    SerialMon.println();
  }

void saveDataToSD(const String& data) {
    SerialMon.println("Saving data to SD card...");
    // Get the current time
    
    // Create a filename based on the current date
    char filename[20];
    sprintf(filename, "debugMQTT.csv"); 
    // sprintf(filename, "/%04d-%02d-%02d.csv", 
    //         ptm->tm_year + 1900, ptm->tm_mon + 1, ptm->tm_mday);

    // Open the file for appending
    File file = SD.open(filename, FILE_APPEND);
    if (!file) {
        SerialMon.println("Failed to open file for writing");
        return;
    }

    // Write the data with the timestamp
    // file.print(timeString);
    // file.print(",");
    file.println(data);
    file.close();

    SerialMon.println("Data saved to SD card");
}

void collectSensorData() {
    // Collect new data from Serial1
    // SerialMon.println("Collecting sensor data...");
    if (SerialUART.available() > 0) {
        // SerialMon.println("Serial1 data available");
        String data = SerialUART.readStringUntil('\n');
        dataQueue.push(data);
        SerialMon.println("Data collected: ");
        SerialMon.println(data);
        delay(100);
        // // Save data to SD card
        // saveDataToSD(data);
    } else {
        SerialMon.println("No data available on Serial1");
    }
}

// Function to validate and translate JSON data
bool validateAndTranslateJson(String& payload) {
    DynamicJsonDocument obj(1024); // Specify an appropriate size for the JSON document
    DeserializationError error = deserializeJson(obj, payload);
    if (error) {
        SerialMon.print(F("deserializeJson() failed: "));
        SerialMon.println(error.f_str());
        return false;
    }

    // Prüfen, ob die Daten ein Array sind
    if (!obj.is<JsonArray>()) {
        SerialMon.println(F("Invalid JSON structure: Expected an array"));
        return false;
    }

    // Überprüfen jedes Objekts im Array
    for (JsonObject obj : obj.as<JsonArray>()) {
        if (!obj["id"].is<int>() || !obj["type"].is<int>() || !obj["data"].is<float>()) {
            SerialMon.println(F("Invalid JSON structure in array element"));
            return false;
        }

        // Übersetzen des numerischen Typs in alphanumerischen Typ
        int type = obj["type"];
        switch (type) {
            case 1:
                obj["type"] = "DATE";
                break;
            case 2:
                obj["type"] = "UTC";
                break;
            case 16:
                obj["type"] = "BOARDTIME_SEC";
                break;
            case 20:
                obj["type"] = "RPM";
                break;
            case 21:
                obj["type"] = "LATITUDE";
                break;
            case 22:
                obj["type"] = "LONGITUDE";
                break;
            case 28:
                obj["type"] = "POSITION_DISTANCE";
                break;
            default:
                SerialMon.println(F("Unknown type in array element"));
                return false;
        }
    }

    // Serialisiere die modifizierten Daten zurück in den Payload
    payload = "";
    serializeJson(obj, payload);
    return true;
}

// Publish data from the queue
void publishSensorData() {
    bool pubOK = false;
    if (!modemInitialized) {
        SerialMon.println("Modem not initialized, skipping MQTT publish");
        return;
    }

    SerialMon.println("Publishing sensor data...");
    while (!dataQueue.empty()) {
        String payload = dataQueue.front();
        dataQueue.pop();
        SerialMon.println("Raw data received: " + payload);
        // Validate and translate JSON data
        if (!validateAndTranslateJson(payload)) {
            SerialMon.println("Invalid data, skipping publish");
            continue;
        }

        if (getTopic(payload) == 1) {
            pubOK = mqtt.publish(topicRpm1, payload.c_str());
        } else if (getTopic(payload) == 5) {
            pubOK = mqtt.publish(topicRpm2, payload.c_str());    
        } else if (getTopic(payload) == 2) {
            pubOK = mqtt.publish(topicPosition, payload.c_str());    
        } else {
            SerialMon.println("Unknown topic"); 
        }
        if (pubOK) {
            SerialMon.println("Data published: ");
            SerialMon.println(payload);
        } else {
            // If publish fails, push the data back to the queue
            dataQueue.push(payload);
            SerialMon.println("Failed to publish data, re-queued: ");
            SerialMon.println(payload);
            break; // Exit the loop to avoid infinite loop
        }
    }
}



void mqttCallback(char *topic, byte *payload, unsigned int len)
{ 
    SerialMon.print("Message arrived [");
    SerialMon.print(topic);
    SerialMon.print("]: ");
    SerialMon.write(payload, len);
    SerialMon.println();

    // Only proceed if incoming message's topic matches
    if (String(topic) == topicLed) {
        ledStatus = !ledStatus;
        digitalWrite(LED_PIN, ledStatus);
        SerialMon.print("ledStatus:");
        SerialMon.println(ledStatus);
        mqtt.publish(topicLedStatus, ledStatus ? "1" : "0");
    }
}

boolean mqttConnect() {
    if (!modemInitialized) {
        SerialMon.println("Modem not initialized, skipping MQTT connect");
        return false;
    }

    SerialMon.print("Connecting to ");
    SerialMon.print(broker);

    // Connect to MQTT Broker
    // boolean status = mqtt.connect("GsmClientTest");
    boolean status = mqtt.connect(clientID);

    // Or, if you want to authenticate MQTT:
    // boolean status = mqtt.connect(clientID, user, password);

    if (status == false) {
        SerialMon.println(" fail");
        return false;
    } else {
        SerialMon.println(" MQTT connected successfully");
    }

    String time = String(millis());
    SerialMon.println(" success");
    mqtt.publish(topicInit, ("Time: " + time).c_str());
    mqtt.subscribe(topicLed);
    return mqtt.connected();
}

void initializeModem() {
    // Initialize modem
    SerialMon.println("Initializing modem...");
    if (!modem.init()) {
        SerialMon.println("Failed to init modem, attempting to continue with restarting");
        modem.restart();
    } else {
        SerialMon.println("Modem successfully initialized");
    }

    String name = modem.getModemName();
    SerialMon.print("Modem Name: ");
    SerialMon.println(name);

    String modemInfo = modem.getModemInfo();
    SerialMon.print("Modem Info: ");
    SerialMon.println(modemInfo);

    // Unlock your SIM card with a PIN if needed
    if (GSM_PIN && modem.getSimStatus() != 3) {
        modem.simUnlock(GSM_PIN);
    }

    SerialMon.print("Waiting for network...");
    if (!modem.waitForNetwork()) {
        SerialMon.println(" fail");
        delay(10000);
        return;
    }
    SerialMon.println(" success");

    if (modem.isNetworkConnected()) {
        SerialMon.println("Network connected");
    }

    // GPRS connection parameters are usually set after network registration
    SerialMon.print(F("Connecting to "));
    SerialMon.print(apn);

    if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
        SerialMon.println(" fail");
        delay(10000);
        return;
    }
    SerialMon.println(" success");

    if (modem.isGprsConnected()) {
        SerialMon.println("GPRS connected");
        modemInitialized = true;
    }
}

void setup() {
    // Set console baud rate
    SerialMon.println("Setup started...");
    SerialMon.begin(115200);
    delay(10);
    
    // Set LED OFF
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, HIGH);
    // Set PWR_PIN to HIGH to start the modem 
    pinMode(PWR_PIN, OUTPUT);
    digitalWrite(PWR_PIN, HIGH);
    // Starting the machine requires at least 1 second of low level, and with a level conversion, the levels are opposite
    delay(1000);
    digitalWrite(PWR_PIN, LOW);

    // Starting SD card 
    // pinMode (SD_SCLK, INPUT_PULLUP);
    // pinMode (SD_MISO, INPUT_PULLUP);
    // pinMode (SD_MOSI, INPUT_PULLUP);
    // pinMode (SD_CS, INPUT_PULLUP);

    // SPI.begin(SD_SCLK, SD_MISO, SD_MOSI, SD_CS);
    // if (!SD.begin(SD_CS)) {
    //     SerialMon.println("SDCard MOUNT FAIL");
    // } else {
    //     uint32_t cardSize = SD.cardSize() / (1024 * 1024);
    //     String str = "SDCard Size: " + String(cardSize) + "MB";
    //     SerialMon.println(str);
    // }
    // SerialMon.println("\nWait...");

    delay(100);

    // Set serial for AT commands (to the modem)
    SerialAT.begin(AT_BAUD, SERIAL_8N1, AT_PIN_RX, AT_PIN_TX);

    // Set serial for external device (UART)
    SerialUART.begin(UART_BAUD, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN);

    // Initialize modem
    initializeModem();

    // MQTT Broker setup
    if (modemInitialized) {
        SerialMon.print("Connecting to ");  
        SerialMon.print(broker);
        mqtt.setServer(broker, port);
        mqtt.setCallback(mqttCallback);
        SerialMon.println(" success");
    }

    // delay(1000);
    
    // SerialMon.println("Looking for the NTP time...");
    // // Init and get the time
    // configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    // printLocalTime();

    delay(500);

    SerialMon.println("Setup completed.");
}

void loop() {
    // Make sure we're still registered on the network
    SerialMon.println("Loop started...");
    if (modemInitialized && !modem.isNetworkConnected()) {
        SerialMon.println("Network disconnected");
        if (!modem.waitForNetwork(180000L, true)) {
            SerialMon.println(" fail");
            delay(100);
            return;
        }
        if (modem.isNetworkConnected()) {
            SerialMon.println("Network re-connected");
        }

        // and make sure GPRS/EPS is still connected
        if (!modem.isGprsConnected()) {
            SerialMon.println("GPRS disconnected!");
            SerialMon.print(F("Connecting to "));
            SerialMon.print(apn);
            if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
                SerialMon.println(" fail");
                delay(1000);
                return;
            }
            if (modem.isGprsConnected()) {
                SerialMon.println("GPRS reconnected");
            }
        }
    }

    if (modemInitialized && !mqtt.connected()) {
        SerialMon.println("=== MQTT NOT CONNECTED ===");
        // Reconnect every 500 milliseconds
        uint32_t t = millis();
        if (t - lastReconnectAttempt > 500L) {
            lastReconnectAttempt = t;
            if (mqttConnect()) {
                lastReconnectAttempt = 0;
            }
        }
        // delay(100);
        return;
    }

    if (modemInitialized) {
        mqtt.loop();
    }

    // Collect new data at regular intervals
    unsigned long currentMillis = millis();

    if (currentMillis - lastDataCollectionTime >= dataCollectionInterval) {
        lastDataCollectionTime = currentMillis;
        collectSensorData();
    }
    // delay(100);
    // Publish new data at regular intervals
    if (currentMillis - lastPublishTime >= publishInterval) {
        lastPublishTime = currentMillis;
        publishSensorData();
    }
    // delay(100);
    // Print the number of unsent data in the queue
    if (dataQueue.size() > 0) {
        SerialMon.print("Unsent data in queue: ");
        SerialMon.println(dataQueue.size());
    }
}