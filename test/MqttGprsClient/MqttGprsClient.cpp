// --------------------------------------------------------------------------------------------
// MQTT GPRS Client
// --------------------------------------------------------------------------------------------
//  The code is a modified version of the  HTTPSClient example from the TinyGSM library. 
//  The code is divided into two main parts: the setup() and loop() functions. 
//  In the setup() function, we initialize the serial monitor, set the LED pin as an output, and power on the SIM7000 module. 
//  We then initialize the serial communication with the SIM7000 module and restart it. 
//  In the loop() function, we set the network mode and preferred mode to NB-IoT. 
//  We then connect to the GPRS network using the APN, username, and password. 
//  Finally, we wait for the network to connect and print a message to the serial monitor. 
//  Testing the NB-IoT Connection 
//  To test the NB-IoT connection, upload the code to your ESP32 board. 
//  Open the serial monitor at a baud rate of 115200. 
//  You should see the following messages: 
//  Wait...

/*
  Rui Santos
  Complete project details at https://RandomNerdTutorials.com/esp32-https-requests-sim-card-sim7000g/
  
  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files.
  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software. Based on the library example: github.com/vshymanskyy/TinyGSM/blob/master/examples/HttpsClient/HttpsClient.ino
*/

// Select your modem
#define TINY_GSM_MODEM_SIM7000SSL
#define TINY_GSM_RX_BUFFER 1024 // Set RX buffer to 1Kb

// Set serial for debug console (to the Serial Monitor, default speed 115200)
#define SerialMon Serial
#define SerialAT Serial1

#include <TinyGsmClient.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <ArduinoJson.hpp>

// Define the serial console for debug prints, if needed
#define TINY_GSM_DEBUG SerialMon


// set GSM PIN, if any
#define GSM_PIN ""

// flag to force SSL client authentication, if needed
// #define TINY_GSM_SSL_CLIENT_AUTHENTICATION

// Set your APN Details / GPRS credentials
const char apn[]      = "hologram";
const char gprsUser[] = "";
const char gprsPass[] = "";

// Server details
const char server[]   = "gist.githubusercontent.com";
const char resource[] = "/RuiSantosdotme/7db8537cef1c84277c268c76a58d07ff/raw/d3fe4cd6eff1ed43e6dbd1883ab7eba8414e2406/gistfile1.txt";
const int  port       = 443;

TinyGsm        modem(SerialAT);

TinyGsmClientSecure client(modem);


// LilyGO T-SIM7000G Pinout
#define UART_BAUD           115200
#define PIN_DTR             25
#define PIN_TX              27
#define PIN_RX              26
#define PWR_PIN             4

#define SD_MISO             2
#define SD_MOSI             15
#define SD_SCLK             14
#define SD_CS               13
#define LED_PIN             12

void modemPowerOn(){
  pinMode(PWR_PIN, OUTPUT);
  digitalWrite(PWR_PIN, LOW);
  delay(1000);
  digitalWrite(PWR_PIN, HIGH);
}

void modemPowerOff(){
  pinMode(PWR_PIN, OUTPUT);
  digitalWrite(PWR_PIN, LOW);
  delay(1500);
  digitalWrite(PWR_PIN, HIGH);
}

void modemRestart(){
  modemPowerOff();
  delay(1000);
  modemPowerOn();
}

String showIP(){
    String ipAddress;
    do {
      ipAddress = modem.getLocalIP();
      delay(1000); // Warte eine Sekunde bevor erneut abgefragt wird
    } while (ipAddress == "0.0.0.0");
    return ipAddress;
}


void reconnect() {
    // Loop until we're reconnected
    while (!client.connected()) {
      Serial.print("Attempting MQTT connection...");
      // Create a random client ID
      String clientId = "TowerStation-";
      clientId += String(random(0xffff), HEX);
      // Attempt to connect
      if (client.connect(clientId.c_str())) {
        Serial.println("connected");
      } else {
        Serial.print("failed, rc=");
        Serial.print(client.state());
        delay(5000);
      }
    }
}

void setup() {
  // Set Serial Monitor baud rate
  SerialMon.begin(115200);
  delay(10);

  // Set LED OFF
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);

  modemPowerOn();

  SerialMon.println("Wait...");

  // Set GSM module baud rate and Pins
  SerialAT.begin(UART_BAUD, SERIAL_8N1, PIN_RX, PIN_TX);
  delay(60);

  // Restart takes quite some time
  // To skip it, call init() instead of restart()
  SerialMon.println("Initializing modem...");
//   modem.restart();
  modem.init();

  Serial.println("Make sure your LTE antenna has been connected to the SIM interface on the board.");
  delay(10);

  String res;
  // CHANGE NETWORK MODE, IF NEEDED
  modem.setNetworkMode(2);
  // CHANGE PREFERRED MODE, IF NEEDED
  modem.setPreferredMode(2);  // 2 = NB-IoT

  modem.gprsConnect(apn, gprsUser, gprsPass);

  SerialMon.print("Waiting for network...");
  if (!modem.waitForNetwork()) {
    SerialMon.println(" fail");
    delay(100);
    return;
  }
    SerialMon.println(" success");
    SerialMon.print("Network connected");
    SerialMon.print(" IP: ");
    SerialMon.println(String (showIP()));
    SerialMon.print(" millSec from start: ");  
    SerialMon.println(millis());  

}

  void loop() {

    if (!client.connected()) {
        reconnect();
      }
    
      StaticJsonDocument<80> doc;
      char output[80];
    
      long now = millis();
      if (now - lastMsg > 5000) {
        lastMsg = now;
        float temp = bme.readTemperature();
        float pressure = bme.readPressure()/100.0;
        float humidity = bme.readHumidity();
        float gas = bme.readGas()/1000.0;
        doc["t"] = temp;
        doc["p"] = pressure;
        doc["h"] = humidity;
        doc["g"] = gas;
    
        serializeJson(doc, output);
        Serial.println(output);
        client.publish("/home/sensors", output);
      }
}
 
