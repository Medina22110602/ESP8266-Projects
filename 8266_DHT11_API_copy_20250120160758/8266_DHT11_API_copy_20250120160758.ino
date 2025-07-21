#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Arduino_JSON.h>
#include <FS.h>
#include "DHT.h"

// Replace with your DHT sensor pin and type
#define DHTPIN 2       // GPIO2 corresponds to D4 on NodeMCU
#define DHTTYPE DHT11  // Specify the DHT sensor type (DHT11)

// Initialize the DHT sensor
DHT dht(DHTPIN, DHTTYPE);

// Replace with your network credentials
const char* ssid = "Intelory WiFi";
const char* password = "LuTa6OF0@INT";

// OPENWEATHER API CONFIGURATION
String openWeatherMapApiKey = "a045af0ad7000a9e2a426c4f5657d16c";
String temperature_unit = "metric";
String city = "Skopje";
String endpoint;

float OutTemperature;
int OutHumidity;
String Icon_;
int Sun_rise;
int Sun_set;
String jsonBuffer;

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);
AsyncEventSource events("/events");

// JSON Variable to Hold Sensor Readings
JSONVar readings;
JSONVar owmreadings;

// Timer variables
unsigned long lastTime = 0;
unsigned long lastTimeTemperature = 0;
unsigned long lastTimeAcc = 0;
unsigned long SensorDelay = 10;
unsigned long OWMapDelay = 1000;

float in_temperature, in_humidity;
float out_temperature, out_humidity, out_weather;
int in_temperatureD, out_temperatureD;

// Initialize SPIFFS
void initSPIFFS() {
  if (!SPIFFS.begin()) {
    Serial.println("An error has occurred while mounting SPIFFS");
  } else {
    Serial.println("SPIFFS mounted successfully");
  }
}

// Initialize WiFi
void initWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  Serial.println("");
  Serial.print("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
  }

  Serial.println("");
  Serial.print("Connected! IP address: ");
  Serial.println(WiFi.localIP());
}

String httpGETRequest(const char* serverName) {
  WiFiClient client;
  HTTPClient http;

  http.begin(client, serverName);
  int httpResponseCode = http.GET();
  String payload = "{}";

  if (httpResponseCode > 0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    payload = http.getString();
  } else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
  }

  http.end();
  return payload;
}

// Read DHT11 sensor data and prepare JSON
String getReadings() {
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();

  // Check if any reads failed and exit early
  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("Failed to read from DHT sensor!");
    return "{}";
  }

  readings["in_temperature"] = String(temperature);
  readings["in_humidity"] = String(humidity);

  String jsonString = JSON.stringify(readings);
  return jsonString;
}

// Fetch OpenWeatherMap readings
String getOWMReadings() {
  if (WiFi.status() == WL_CONNECTED) {
    endpoint = "http://api.openweathermap.org/data/2.5/weather?q=" + city + "&units=" + temperature_unit + "&appid=" + openWeatherMapApiKey;
    jsonBuffer = httpGETRequest(endpoint.c_str());

    JSONVar myObject = JSON.parse(jsonBuffer);

    if (JSON.typeof(myObject) == "undefined") {
      Serial.println("Parsing input failed!");
      return "{}";
    }

    Icon_ = String((const char*)myObject["weather"][0]["icon"]);
    OutTemperature = double(myObject["main"]["temp"]);
    OutHumidity = int(myObject["main"]["humidity"]);
    Sun_rise = int(myObject["sys"]["sunrise"]);
    Sun_set = int(myObject["sys"]["sunset"]);

    owmreadings["out_temperature"] = String(OutTemperature);
    owmreadings["out_humidity"] = String(OutHumidity);
    owmreadings["weather_icon"] = String("icons/" + Icon_ + ".png");
    owmreadings["Sunset"] = String(Sun_set);
    owmreadings["Sunrise"] = String(Sun_rise);
    owmreadings["City"] = String(city);

    String jsonString = JSON.stringify(owmreadings);
    Serial.println(jsonString);
    return jsonString;
  } else {
    Serial.println("WiFi Disconnected");
    return "{}";
  }
}

void setup() {
  Serial.begin(115200);

  // Initialize WiFi, SPIFFS, and DHT sensor
  initWiFi();
  initSPIFFS();
  dht.begin();

  // Handle Web Server
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(SPIFFS, "/index.html", "text/html");
  });

  server.serveStatic("/", SPIFFS, "/");

  server.on("/reset", HTTP_GET, [](AsyncWebServerRequest* request) {
    in_temperature = 0;
    in_humidity = 0;
    out_temperature = 0;
    out_humidity = 0;
    request->send(200, "text/plain", "OK");
  });

  events.onConnect([](AsyncEventSourceClient* client) {
    if (client->lastId()) {
      Serial.printf("Client reconnected! Last message ID that it got is: %u\n", client->lastId());
    }
    client->send("Connected!", NULL, millis(), 10000);
  });

  server.addHandler(&events);
  server.begin();
}

void loop() {
  if ((millis() - lastTime) > SensorDelay) {
    events.send(getReadings().c_str(), "DHT_readings", millis());
    lastTime = millis();
  }

  if ((millis() - lastTimeAcc) > OWMapDelay) {
    events.send(getOWMReadings().c_str(), "OWM_readings", millis());
    lastTimeAcc = millis();
  }
}
