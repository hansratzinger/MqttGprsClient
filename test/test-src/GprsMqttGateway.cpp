

// Set serial for debug console (to the Serial Monitor, default speed 115200)
#define SerialMon Serial

// Set serial for AT commands (to the SIM7000 module)
#define MODEM_RST 5
#define MODEM_PWRKEY 4
#define MODEM_POWER_ON 23
#define MODEM_TX 27
#define MODEM_RX 26

// Set serial for UART input
#define UART_INPUT_TX 13
#define UART_INPUT_RX 14

// Configure TinyGSM library
#define TINY_GSM_MODEM_SIM7000
#define TINY_GSM_USE_GPRS true
#define TINY_GSM_USE_WIFI false

// Your GPRS credentials
const char apn[] = "hologram";
const char gprsUser[] = "";
const char gprsPass[] = "";

// MQTT Broker details
const char* broker = "194c32edd2ba406fb8ecdba22f6f7816.s1.eu.hivemq.cloud";
const char* topic = "boat/rpm";

#include <Arduino.h>
#include <TinyGsmClient.h>
#include <PubSubClient.h>

HardwareSerial SerialAT(1); // Use UART1 for the modem
HardwareSerial SerialUART(2); // Use UART2 for UART input


TinyGsm modem(SerialAT);
TinyGsmClient client(modem);
PubSubClient mqtt(client);

void setup();
void loop();
void reconnect();
void mqttCallback(char* topic, byte* payload, unsigned int length);

void setup() {
  // Set console baud rate
  SerialMon.begin(115200);
  delay(10);

  // Set GSM module baud rate
  SerialAT.begin(9600, SERIAL_8N1, MODEM_RX, MODEM_TX);
  delay(3000);

  // Set UART input baud rate
  SerialUART.begin(9600, SERIAL_8N1, UART_INPUT_RX, UART_INPUT_TX);

  // Restart takes quite some time
  // To skip it, call init() instead of restart()
  SerialMon.println("Initializing modem...");
  modem.restart();

  // Unlock your SIM card with a PIN if needed
  // modem.simUnlock("1234");

  SerialMon.print("Connecting to APN: ");
  SerialMon.println(apn);
  if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
    SerialMon.println("Failed to connect to GPRS");
    while (true);
  }
  SerialMon.println("GPRS connected");

  mqtt.setServer(broker, 1883);
  mqtt.setCallback(mqttCallback);

  // if (!mqtt.connect("ESP32Client")) {
  //   SerialMon.println("Failed to connect to MQTT broker");
  //   while (true);
  // }
  // SerialMon.println("MQTT connected");
}

void loop() {
  if (!mqtt.connected()) {
    reconnect();
  }
  mqtt.loop();

  if (SerialUART.available()) {
    String message = SerialUART.readStringUntil('\n');
    mqtt.publish(topic, message.c_str());
  }
}

void reconnect() {
  while (!mqtt.connected()) {
    SerialMon.print("Attempting MQTT connection...");
    if (mqtt.connect("ESP32Client")) {
      SerialMon.println("MQTT connected");
    } else {
      SerialMon.print("failed, rc=");
      SerialMon.print(mqtt.state());
      SerialMon.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  SerialMon.print("Message arrived [");
  SerialMon.print(topic);
  SerialMon.print("] ");
  for (unsigned int i = 0; i < length; i++) {
    SerialMon.print((char)payload[i]);
  }
  SerialMon.println();
}