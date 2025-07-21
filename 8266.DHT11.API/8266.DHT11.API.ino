#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <Hash.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>

// OpenWeather API Configuration
String openWeatherMapApiKey = "a045af0ad7000a9e2a426c4f5657d16c";  // Your OpenWeather API key
String city = "Skopje";  
String weatherApiUrl = "http://api.openweathermap.org/data/2.5/weather?q=" + city + "&appid=" + openWeatherMapApiKey + "&units=metric";

// Wi-Fi credentials
const char* ssid = "Intelory WiFi";         // Replace with your Wi-Fi SSID
const char* password = "LuTa6OF0@INT";      // Replace with your Wi-Fi password

#define DHTPIN 2    // Digital pin connected to the DHT sensor
#define DHTTYPE DHT11 // DHT 11 sensor type

DHT dht(DHTPIN, DHTTYPE);

// Indoor temperature and humidity
float indoorTemp = 0.0;
float indoorHumidity = 0.0;

// Outdoor weather data
float outdoorTemp = 0.0;
float outdoorHumidity = 0.0;
String weatherDescription = "";

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

// Time-related variables
unsigned long previousMillis = 0;
const long interval = 10000;  // Update interval for DHT sensor and API

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.5.1/css/all.min.css">
  <style>
    html { font-family: Arial; display: inline-block; margin: 0px auto; text-align: center; }
    h2 { font-size: 3.0rem; }
    p { font-size: 2.5rem; }
    .units { font-size: 1.2rem; }
    .dht-labels { font-size: 1.5rem; vertical-align: middle; padding-bottom: 15px; }
  </style>
</head>
<body>
  <h2>ESP8266 Weather Station</h2>
  <p><i class="fa-solid fa-temperature-quarter" style="color: #7eb13e;"></i>
    <span class="dht-labels">Indoor Temperature</span>
    <span id="indoorTemp">%INDOOR_TEMP%</span><sup class="units">&deg;C</sup></p>
  <p><i class="fa-solid fa-droplet" style="color: #74C0FC;"></i>
    <span class="dht-labels">Indoor Humidity</span>
    <span id="indoorHumidity">%INDOOR_HUMIDITY%</span><sup class="units">%</sup></p>
  <p><i class="fa-solid fa-temperature-half" style="color: #ffa500;"></i>
    <span class="dht-labels">Outdoor Temperature</span>
    <span id="outdoorTemp">%OUTDOOR_TEMP%</span><sup class="units">&deg;C</sup></p>
  <p><i class="fa-solid fa-droplet" style="color: #1e90ff;"></i>
    <span class="dht-labels">Outdoor Humidity</span>
    <span id="outdoorHumidity">%OUTDOOR_HUMIDITY%</span><sup class="units">%</sup></p>
  <p><i class="fa-solid fa-cloud" style="color: #74C0FC;"></i>
    <span class="dht-labels">Weather</span>
    <span id="weather">%WEATHER%</span></p>
</body>
<script>
setInterval(function () {
  fetch('/indoorTemp').then(response => response.text()).then(data => {
    document.getElementById("indoorTemp").innerHTML = data;
  });
  fetch('/indoorHumidity').then(response => response.text()).then(data => {
    document.getElementById("indoorHumidity").innerHTML = data;
  });
  fetch('/outdoorTemp').then(response => response.text()).then(data => {
    document.getElementById("outdoorTemp").innerHTML = data;
  });
  fetch('/outdoorHumidity').then(response => response.text()).then(data => {
    document.getElementById("outdoorHumidity").innerHTML = data;
  });
  fetch('/weather').then(response => response.text()).then(data => {
    document.getElementById("weather").innerHTML = data;
  });
}, 10000);
</script>
</html>)rawliteral";

// Replaces placeholder with values
String processor(const String& var) {
  if (var == "INDOOR_TEMP") {
    return String(indoorTemp);
  } else if (var == "INDOOR_HUMIDITY") {
    return String(indoorHumidity);
  } else if (var == "OUTDOOR_TEMP") {
    return String(outdoorTemp);
  } else if (var == "OUTDOOR_HUMIDITY") {
    return String(outdoorHumidity);
  } else if (var == "WEATHER") {
    return weatherDescription;
  }
  return String();
}

// Fetch weather data from OpenWeather API
void updateOutdoorWeather() {
  WiFiClient client;
  HTTPClient http;

  http.begin(client, weatherApiUrl);
  int httpCode = http.GET();

  if (httpCode > 0) {
    String payload = http.getString();
    DynamicJsonDocument doc(2048);
    deserializeJson(doc, payload);

    // Extract data
    outdoorTemp = doc["main"]["temp"].as<float>();
    outdoorHumidity = doc["main"]["humidity"].as<float>();
    weatherDescription = doc["weather"][0]["description"].as<String>();
  } else {
    weatherDescription = "API Error";
  }

  http.end();
}

void setup() {
  Serial.begin(115200);
  dht.begin();

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("\nConnected to Wi-Fi");
  Serial.println(WiFi.localIP());

  // Setup server routes
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", index_html, processor);
  });
  server.on("/indoorTemp", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", String(indoorTemp).c_str());
  });
  server.on("/indoorHumidity", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", String(indoorHumidity).c_str());
  });
  server.on("/outdoorTemp", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", String(outdoorTemp).c_str());
  });
  server.on("/outdoorHumidity", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", String(outdoorHumidity).c_str());
  });
  server.on("/weather", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", weatherDescription.c_str());
  });

  server.begin();
}

void loop() {
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    // Update indoor readings
    float newTemp = dht.readTemperature();
    float newHumidity = dht.readHumidity();

    if (!isnan(newTemp)) {
      indoorTemp = newTemp;
    }
    if (!isnan(newHumidity)) {
      indoorHumidity = newHumidity;
    }

    // Update outdoor readings
    updateOutdoorWeather();
  }
}
