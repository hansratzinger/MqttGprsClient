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

std::queue<String> dataQueueId1; // Queue für Daten von id:1
std::queue<String> dataQueueId2; // Queue für Daten von id:2

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
    if (SerialUART.available() > 0) {
        String data = SerialUART.readStringUntil('\n');
        SerialMon.println("Raw data collected: " + data);

        // Prüfen, ob die Daten gültig sind
        StaticJsonDocument<512> doc;
        DeserializationError error = deserializeJson(doc, data);
        if (error) {
            SerialMon.print("Invalid JSON data received: ");
            SerialMon.println(error.f_str());
            return;
        }

        // Prüfen, ob die Daten ein Array sind
        if (!doc.is<JsonArray>()) {
            SerialMon.println("Invalid JSON structure: Expected an array");
            return;
        }

        // Iterieren durch die Elemente des Arrays
        for (JsonObject obj : doc.as<JsonArray>()) {
            if (!obj.containsKey("id")) {
                SerialMon.println("Missing 'id' field in JSON object");
                continue;
            }

            int id = obj["id"];
            String element;
            serializeJson(obj, element);

            if (id == 1) {
                dataQueueId1.push(element);
                SerialMon.println("Data added to queue for id:1: " + element);
            } else if (id == 2) {
                dataQueueId2.push(element);
                SerialMon.println("Data added to queue for id:2: " + element);
            } else {
                SerialMon.println("Unknown ID, skipping data");
            }
        }
    } else {
        SerialMon.println("No data available on SerialUART");
    }
}

// Function to validate and translate JSON data
bool validateAndTranslateJson(String& payload) {
    DynamicJsonDocument doc(1024); // Verwenden Sie DynamicJsonDocument
    DeserializationError error = deserializeJson(doc, payload);
    if (error) {
        SerialMon.print(F("deserializeJson() failed: "));
        SerialMon.println(error.f_str());
        return false;
    }

    // Prüfen, ob die Daten ein einzelnes Objekt oder ein Array sind
    if (doc.is<JsonObject>()) {
        JsonObject obj = doc.as<JsonObject>();

        // Prüfen, ob die erforderlichen Felder vorhanden sind
        if (!obj["id"].is<int>() || !obj["type"].is<int>() || !obj["data"].is<float>()) {
            SerialMon.println(F("Invalid JSON structure in object"));
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
                SerialMon.println(F("Unknown type in object"));
                return false;
        }
    } else if (doc.is<JsonArray>()) {
        SerialMon.println(F("Unexpected JSON structure: Array received, but object expected"));
        return false;
    } else {
        SerialMon.println(F("Invalid JSON structure: Neither object nor array"));
        return false;
    }

    // Serialisiere die modifizierten Daten zurück in den Payload
    payload = "";
    serializeJson(doc, payload);
    return true;
}

// Publish data from the queue
void publishSensorData() {
    bool pubOK = false;

    // JSON-Objekt für id:1 erstellen
    DynamicJsonDocument id1Doc(512);
    id1Doc["id"] = 1;
    JsonObject id1Data = id1Doc.createNestedObject("data");

    // Verarbeite die Daten von id:1
    while (!dataQueueId1.empty()) {
        String payload = dataQueueId1.front();
        dataQueueId1.pop();
        SerialMon.println("Processing data for id:1: " + payload);

        if (!validateAndTranslateJson(payload)) {
            SerialMon.println("Invalid data for id:1, skipping publish");
            continue;
        }

        // JSON-Daten des aktuellen Elements parsen
        DynamicJsonDocument tempDoc(256);
        deserializeJson(tempDoc, payload);
        JsonObject obj = tempDoc.as<JsonObject>();

        // Füge den `type`-Wert als Schlüssel hinzu
        const char* type = obj["type"];
        id1Data[type] = obj["data"];
    }

    // Veröffentliche die gesammelten Daten von id:1, wenn sie nicht leer sind
    if (!id1Data.isNull() && id1Data.size() > 0) {
        String output;
        serializeJson(id1Doc, output);
        pubOK = mqtt.publish(topicRpm1, output.c_str());
        if (pubOK) {
            SerialMon.println("Data published for id:1: " + output);
        } else {
            SerialMon.println("Failed to publish data for id:1");
        }
    } else {
        SerialMon.println("No valid data for id:1, skipping publish");
    }

    // JSON-Objekt für id:2 erstellen
    DynamicJsonDocument id2Doc(512);
    id2Doc["id"] = 2;
    JsonObject id2Data = id2Doc.createNestedObject("data");

    // Verarbeite die Daten von id:2
    while (!dataQueueId2.empty()) {
        String payload = dataQueueId2.front();
        dataQueueId2.pop();
        SerialMon.println("Processing data for id:2: " + payload);

        if (!validateAndTranslateJson(payload)) {
            SerialMon.println("Invalid data for id:2, skipping publish");
            continue;
        }

        // JSON-Daten des aktuellen Elements parsen
        DynamicJsonDocument tempDoc(256);
        deserializeJson(tempDoc, payload);
        JsonObject obj = tempDoc.as<JsonObject>();

        // Füge den `type`-Wert als Schlüssel hinzu
        const char* type = obj["type"];
        id2Data[type] = obj["data"];
    }

    // Veröffentliche die gesammelten Daten von id:2, wenn sie nicht leer sind
    if (!id2Data.isNull() && id2Data.size() > 0) {
        String output;
        serializeJson(id2Doc, output);
        pubOK = mqtt.publish(topicPosition, output.c_str());
        if (pubOK) {
            SerialMon.println("Data published for id:2: " + output);
        } else {
            SerialMon.println("Failed to publish data for id:2");
        }
    } else {
        SerialMon.println("No valid data for id:2, skipping publish");
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
    SerialMon.println("=====================================");
    SerialMon.println("======= MQTT-GPRS-CLIENT ============");
    SerialMon.println("=====================================");
    delay(1000);

    // Set LED OFF
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, HIGH);
    // Set PWR_PIN to HIGH to start the modem 
    pinMode(PWR_PIN, OUTPUT);
    digitalWrite(PWR_PIN, HIGH);
    // Starting the machine requires at least 1 second of low level, and with a level conversion, the levels are opposite
    delay(1000);
    digitalWrite(PWR_PIN, LOW);

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

    delay(500);

    SerialMon.println("Setup completed.");
}

void loop() {
    // Make sure we're still registered on the network
    // SerialMon.println("Loop started...");
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

    // Publish new data at regular intervals
    if (currentMillis - lastPublishTime >= publishInterval) {
        lastPublishTime = currentMillis;
        publishSensorData();
    }

    // Print the number of unsent data in the queues
    if (!dataQueueId1.empty() || !dataQueueId2.empty()) {
        SerialMon.print("Unsent data in queue for id:1: ");
        SerialMon.println(dataQueueId1.size());
        SerialMon.print("Unsent data in queue for id:2: ");
        SerialMon.println(dataQueueId2.size());
    }
}