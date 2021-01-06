/*
 * Copyright (c) 2018, circuits4you.com
 * All rights reserved.
 * Create a TCP Server on ESP8266 NodeMCU. 
 * TCP Socket Server Send Receive Demo
 */

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

#define LEFT 14
#define RIGHT 12
#define FORWARD 13
#define BACKWARD 4

int port = 8888;  //Port number
WiFiServer server(port);

//Server connect to WiFi Network
const char *ssid = "XXXXX";  //Enter your wifi SSID
const char *password = "XXXXXXXXXX";  //Enter your wifi Password

int count=0;
//=======================================================================
//                    Power on setup
//=======================================================================
void setup() 
{
	pinMode(LEFT, INPUT_PULLUP);
	pinMode(RIGHT, INPUT_PULLUP);
	pinMode(FORWARD, INPUT_PULLUP);
	pinMode(BACKWARD, INPUT_PULLUP);
	pinMode(LED_BUILTIN, OUTPUT);
	digitalWrite(LED_BUILTIN, HIGH);   // D1 Mini: turns the LED off, joy led on
 
	Serial.begin(115200);
	Serial.println();

	WiFi.mode(WIFI_STA);
	WiFi.begin(ssid, password); //Connect to wifi
 
	// Wait for connection  
	Serial.println("ESP8266 Joystick v1.0, Connecting to Wifi");
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
		delay(500);
	}

  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  // ArduinoOTA.setHostname("myesp8266");

  // No authentication by default
  // ArduinoOTA.setPassword("admin");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_FS
      type = "filesystem";
    }

    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();
	Serial.println("");
	Serial.print("Connected to ");
	Serial.println(ssid);

	Serial.print("IP address: ");
	Serial.println(WiFi.localIP());
	server.begin();
	Serial.print("Open Telnet and connect to IP:");
	Serial.print(WiFi.localIP());
	Serial.print(" on port ");
	Serial.println(port);
}
//=======================================================================
//                    Loop
//=======================================================================

void loop() 
{
	WiFiClient client = server.available();
	uint32_t cntr, cntrnotsent = 0, inputs;
 
	ArduinoOTA.handle();

	if (client) {
		if (client.connected()) {
			Serial.println("Client Connected");
		}

		while (client.connected()) {
			while (client.available() > 0) {
				// read data from the connected client
				Serial.write(client.read()); 
			}
			//Send Data to connected client
			inputs = ( ~ (digitalRead(LEFT) | digitalRead(RIGHT) << 1 |
									 digitalRead(FORWARD) << 2 | digitalRead(BACKWARD) << 3)) & 0x0f;
			if (inputs) {
				client.write('0' + inputs);  /* send and ascii '0' with added LS 4 bits */
				cntrnotsent = 0;
			}
			delay(10);  /* in milliseconds */
			if (cntrnotsent++ >= 500) { /* send a ping every 5 sec's */
				client.write('0');  /* send ping */
				cntrnotsent = 0;
			}
			digitalWrite(LED_BUILTIN, cntr++ & (1 << 7) ? LOW : HIGH);  /* flash at 1 Hz */
		}
		client.stop();
		digitalWrite(LED_BUILTIN, HIGH);   // D1 Mini: turns the LED off, joy led on
		Serial.println("Client disconnected");
	}
}
//=======================================================================
