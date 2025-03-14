/**************************************************************
 *
 * For this example, you need to install PubSubClient library:
 *   https://github.com/knolleary/pubsubclient
 *   or from http://librarymanager/all#PubSubClient
 *
 * TinyGSM Getting Started guide:
 *   https://tiny.cc/tinygsm-readme
 *
 * For more MQTT examples, see PubSubClient library
 *
 **************************************************************
 * This example connects to HiveMQ's showcase broker.
 *
 * You can quickly test sending and receiving messages from the HiveMQ webclient
 * available at http://www.hivemq.com/demos/websocket-client/.
 *
 * Subscribe to the topic GsmClientTest/ledStatus
 * Publish "toggle" to the topic GsmClientTest/led and the LED on your board
 * should toggle and you should see a new message published to
 * GsmClientTest/ledStatus with the newest LED status.
 *
 **************************************************************/

 #define TINY_GSM_MODEM_SIM7000

 // Set serial for debug console (to the Serial Monitor, default speed 115200)
 #define SerialMon Serial
 
 // Set serial for AT commands (to the module)
 // Use Hardware Serial on Mega, Leonardo, Micro
 #define SerialAT Serial1
 
 // See all AT commands, if wanted
 #define DUMP_AT_COMMANDS
 
 // Define the serial console for debug prints, if needed
 #define TINY_GSM_DEBUG SerialMon
 
 // Add a reception delay, if needed.
 // This may be needed for a fast processor at a slow baud rate.
 // #define TINY_GSM_YIELD() { delay(2); }
 
 // Define how you're planning to connect to the internet
 // These defines are only for this example; they are not needed in other code.
 #define TINY_GSM_USE_GPRS true
 #define TINY_GSM_USE_WIFI false
 
 
 // set GSM PIN, if any
 #define GSM_PIN ""
 
 // Your GPRS credentials, if any
 const char apn[]      = "hologram";
 const char gprsUser[] = "";
 const char gprsPass[] = "";
 
 // MQTT details
 const char *broker = "test.mosquitto.org";
 
 const char *topicLed       = "GsmClientTest/led";
 const char *topicInit      = "GsmClientTest/init";
 const char *topicLedStatus = "GsmClientTest/ledStatus";
 
 #include <TinyGsmClient.h>
 #include <PubSubClient.h>
 #include <Ticker.h>
 #include <SPI.h>
 #include <SD.h>
 #include <ArduinoJson.h>
 
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
  
 #define uS_TO_S_FACTOR 1000000ULL  // Conversion factor for micro seconds to seconds
 #define TIME_TO_SLEEP  10          // Time ESP32 will go to sleep (in seconds)
 
 #define UART_BAUD   115200
 #define PIN_DTR     25
 #define PIN_TX      27
 #define PIN_RX      26
 #define PWR_PIN     4
 
 #define SD_MISO     2
 #define SD_MOSI     15
 #define SD_SCLK     14
 #define SD_CS       13
 #define LED_PIN     12
 
 int ledStatus = LOW;
 unsigned long lastPublishTime = 0; // Variable to store the last publish time
 const unsigned long publishInterval = 100; // Interval to publish data (e.g., every 5 seconds)
 
 uint32_t lastReconnectAttempt = 0;
 
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
 
 boolean mqttConnect()
 {
     SerialMon.print("Connecting to ");
     SerialMon.print(broker);
 
     // Connect to MQTT Broker
     boolean status = mqtt.connect("GsmClientTest");
 
     // Or, if you want to authenticate MQTT:
     // boolean status = mqtt.connect("GsmClientName", "mqtt_user", "mqtt_pass");
 
     if (status == false) {
         SerialMon.println(" fail");
         return false;
     }
     String time = String(millis());
     SerialMon.println(" success");
     mqtt.publish(topicInit, ("Time: " + time).c_str());
     mqtt.subscribe(topicLed);
     return mqtt.connected();
 }

  void publishSensorData() {
     // Collect new data
     float temperature = random(-10,40);
     float pressure = random(950,1050);
     float humidity = random(0,100);
     float rpm = random(0,5000);
     // Create JSON payload
     StaticJsonDocument<200> doc;
     doc["temperature"] = temperature;
     doc["pressure"] = pressure;
     doc["humidity"] = humidity;
     doc["rpm"] = rpm;
     char payload[200];
     serializeJson(doc, payload);
 
     // Publish data
     mqtt.publish("GsmClientTest/sensorData", payload);
     SerialMon.println("Data published: ");
     SerialMon.println(payload);
 }

 void setup()
 {
     // Set console baud rate
     Serial.begin(115200);
     delay(10);
 
     // Set LED OFF
     pinMode(LED_PIN, OUTPUT);
     digitalWrite(LED_PIN, HIGH);
 
     pinMode(PWR_PIN, OUTPUT);
     digitalWrite(PWR_PIN, HIGH);
     // Starting the machine requires at least 1 second of low level, and with a level conversion, the levels are opposite
     delay(1000);
     digitalWrite(PWR_PIN, LOW);
 
     SPI.begin(SD_SCLK, SD_MISO, SD_MOSI, SD_CS);
     if (!SD.begin(SD_CS)) {
         Serial.println("SDCard MOUNT FAIL");
     } else {
         uint32_t cardSize = SD.cardSize() / (1024 * 1024);
         String str = "SDCard Size: " + String(cardSize) + "MB";
         Serial.println(str);
     }
 
     Serial.println("\nWait...");
 
     delay(1000);
 
     SerialAT.begin(UART_BAUD, SERIAL_8N1, PIN_RX, PIN_TX);
 
     // Restart takes quite some time
     // To skip it, call init() instead of restart()
     Serial.println("Initializing modem...");
     if (!modem.restart()) {
         Serial.println("Failed to restart modem, attempting to continue without restarting");
     }
 
 
 
     String name = modem.getModemName();
     DBG("Modem Name:", name);
 
     String modemInfo = modem.getModemInfo();
     DBG("Modem Info:", modemInfo);
 
 
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
     }

 
     // MQTT Broker setup
     mqtt.setServer(broker, 1883);
     mqtt.setCallback(mqttCallback);
 
 }
 
 
void loop()
{
    // Make sure we're still registered on the network
    if (!modem.isNetworkConnected()) {
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
                delay(10000);
                return;
            }
            if (modem.isGprsConnected()) {
                SerialMon.println("GPRS reconnected");
            }
        }
    }

    if (!mqtt.connected()) {
        SerialMon.println("=== MQTT NOT CONNECTED ===");
        // Reconnect every 500 milliseconds
        uint32_t t = millis();
        if (t - lastReconnectAttempt > 500L) {
            lastReconnectAttempt = t;
            if (mqttConnect()) {
                lastReconnectAttempt = 0;
            }
        }
        delay(100);
        return;
    }

    mqtt.loop();

    // Publish new data at regular intervals
    unsigned long currentMillis = millis();
    if (currentMillis - lastPublishTime >= publishInterval) {
        lastPublishTime = currentMillis;
        publishSensorData();
    }
}