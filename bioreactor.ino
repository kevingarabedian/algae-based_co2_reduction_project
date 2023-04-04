#include <OneWire.h>
#include <DallasTemperature.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

// WiFi and server information
const char* ssid = "your_SSID";
const char* password = "your_PASSWORD";
const char* server = "your_SERVER_ADDRESS";
const char* bearerToken = "your_BEARER_TOKEN";

// Temperature sensors
#define TEMP_INSIDE_PIN D2
#define TEMP_OUTSIDE_PIN D3
OneWire oneWireInside(TEMP_INSIDE_PIN);
OneWire oneWireOutside(TEMP_OUTSIDE_PIN);
DallasTemperature tempSensorInside(&oneWireInside);
DallasTemperature tempSensorOutside(&oneWireOutside);

// LED light control
#define LED_PIN D1
const int LED_ON = 1023; // Full brightness
const int LED_OFF = 0;   // Turn off

// Food control
#define FOOD_TRIGGER_PIN D4
#define FOOD_VALVE_PIN D5
#define FLOW_METER_PIN D6
volatile int flowPulses = 0;

// WiFi client and HTTP client
WiFiClient client;
HTTPClient http;

void setup() {
  Serial.begin(115200);
  delay(10);

  // Start temperature sensors
  tempSensorInside.begin();
  tempSensorOutside.begin();

  // Set up LED pin for PWM output
  pinMode(LED_PIN, OUTPUT);

  // Set up food control pins
  pinMode(FOOD_TRIGGER_PIN, INPUT_PULLUP);
  pinMode(FOOD_VALVE_PIN, OUTPUT);
  digitalWrite(FOOD_VALVE_PIN, LOW);

  // Set up flow meter pin
  pinMode(FLOW_METER_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(FLOW_METER_PIN), countPulses, RISING);

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Connected to Wi-Fi");
}

void loop() {
  // Read temperatures
  tempSensorInside.requestTemperatures();
  tempSensorOutside.requestTemperatures();
  float insideTemp = tempSensorInside.getTempCByIndex(0);
  float outsideTemp = tempSensorOutside.getTempCByIndex(0);

  // Control LED light based on outside temperature
  if (outsideTemp < 18) { // Adjust the threshold temperature as needed
    analogWrite(LED_PIN, LED_ON);
  } else {
    analogWrite(LED_PIN, LED_OFF);
  }

  // Check if food trigger is activated
  if (digitalRead(FOOD_TRIGGER_PIN) == LOW) {
    releaseFood();
  }

  // Send data to the server
  sendData(insideTemp, outsideTemp);

  delay(60000); // Wait for 1 minute before taking the next readings
}

void countPulses() {
  flowPulses++;
}

void releaseFood() {
  // Open the solenoid valve
  digitalWrite(FOOD_VALVE_PIN, HIGH);

  // Reset flow meter pulse count
  flowPulses = 0;

  // Wait for the desired amount of food to flow
  while (flowPulses < 500) { // Adjust this value based on the desired amount of food
    delay(10);
  }

  // Close the solenoid valve
  digitalWrite(FOOD_VALVE_PIN, LOW);
}

void sendData(float insideTemp, float outsideTemp) {
  if (client.connect(server  , 80)) {
    // Prepare the payload
    String payload = "insideTemp=" + String(insideTemp) +
                     "&outsideTemp=" + String(outsideTemp) +
                     "&flowPulses=" + String(flowPulses);

    // Prepare the HTTP request
    http.begin(client, server);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    http.addHeader("Authorization", "Bearer " + String(bearerToken));

    // Send the HTTP POST request
    int httpResponseCode = http.POST(payload);

    // Check the response
    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.println("HTTP Response code: " + String(httpResponseCode));
      Serial.println(response);
    } else {
      Serial.println("Error on sending POST: " + String(httpResponseCode));
    }

    // End the HTTP connection
    http.end();
  } else {
    Serial.println("Error connecting to the server");
  }
}
