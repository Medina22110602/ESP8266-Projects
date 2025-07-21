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

// API and Wi-Fi settings
String openWeatherMapApiKey = "628addfac14f06822bc3a33193e1d424";
String city = "Skopje";
String weatherApiUrl = "http://api.openweathermap.org/data/2.5/weather?q=" + city + "&appid=" + openWeatherMapApiKey + "&units=metric";

// ThingSpeak settings
String thingSpeakApiKey = "3S1WYTERP19ERPPL";
unsigned long thingSpeakChannelID = 2978168;
String thingSpeakServer = "api.thingspeak.com";

const char* ssid = "Technoperia_42";
const char* password = "M4a*6@r8";

// DHT Sensor setup
#define DHTPIN 4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// Variables
float indoorTemp = 0.0;
float indoorHumidity = 0.0;
float outdoorTemp = 0.0;
float outdoorHumidity = 0.0;
String weatherDescription = "";
String weatherRecommendation = "";
String lastUpdated = "";

AsyncWebServer server(80);

unsigned long previousMillis = 0;
unsigned long previousThingSpeakMillis = 0;
const long interval = 10000; // Sensor reading interval (10 seconds)
const long thingSpeakInterval = 60000; // ThingSpeak upload interval (60 seconds - ThingSpeak limit)

// Weather Recommendation Generator
String getOutdoorRecommendation(String weather, float temp) {
  weather.toLowerCase();
  if (weather.indexOf("rain") >= 0) return "It might rain today. You should take an umbrella.";
  if (weather.indexOf("snow") >= 0) return "Snow expected. Drive carefully and dress warmly.";
  if (weather.indexOf("thunderstorm") >= 0) return "Thunderstorms possible. Stay indoors if possible.";
  if (weather.indexOf("fog") >= 0 || weather.indexOf("mist") >= 0) return "Reduced visibility due to fog. Drive with caution.";
  if (temp < 5.0) return "Very cold outside. Bundle up and protect exposed skin.";
  if (temp < 15.0) return "It's cold outside. Consider bringing a jacket or coat.";
  if (temp > 35.0) return "Extremely hot! Stay hydrated and avoid prolonged sun exposure.";
  if (temp > 28.0) return "It's hot outside. Wear light, breathable clothes and stay hydrated.";
  if (temp >= 20.0 && temp <= 25.0) return "Perfect weather! Great day to spend time outdoors.";
  return "Weather looks okay. Enjoy your day!";
}

// Updates weather recommendation based on latest outdoor data
void updateWeatherRecommendation() {
  weatherRecommendation = getOutdoorRecommendation(weatherDescription, outdoorTemp);
  
  // Update timestamp
  unsigned long currentTime = millis();
  int hours = (currentTime / 3600000) % 24;
  int minutes = (currentTime / 60000) % 60;
  lastUpdated = String(hours) + ":" + (minutes < 10 ? "0" : "") + String(minutes);
}

// Get comfort level based on temperature and humidity
String getComfortLevel(float temp, float humidity) {
  if (temp < 18.0) return "Too Cold";
  if (temp > 26.0) return "Too Warm";
  if (humidity < 30.0) return "Too Dry";
  if (humidity > 60.0) return "Too Humid";
  if (temp >= 20.0 && temp <= 24.0 && humidity >= 40.0 && humidity <= 50.0) return "Perfect";
  return "Comfortable";
}

// Enhanced HTML Page with desktop-friendly design
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <title>ESP8266 Weather Station</title>
  <script src="https://cdn.tailwindcss.com"></script>
  <link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.0.0/css/all.min.css" />
  <style>
    .gradient-bg { background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); }
    .card-hover { transition: all 0.3s ease; }
    .card-hover:hover { transform: translateY(-2px); box-shadow: 0 8px 25px rgba(0,0,0,0.15); }
    .pulse { animation: pulse 2s infinite; }
    @keyframes pulse { 0%, 100% { opacity: 1; } 50% { opacity: 0.7; } }
    .status-indicator { width: 12px; height: 12px; border-radius: 50%; display: inline-block; margin-right: 8px; }
    .status-online { background-color: #10b981; box-shadow: 0 0 8px rgba(16, 185, 129, 0.4); }
    .temp-display { font-size: 3.5rem; font-weight: 700; line-height: 1; }
    .metric-value { font-size: 2.5rem; font-weight: 600; }
  </style>
</head>
<body class="min-h-screen gradient-bg">
  <div class="container mx-auto px-6 py-8 max-w-7xl">
    <!-- Header -->
    <div class="text-center mb-8">
      <h1 class="text-5xl font-bold text-white mb-2">
        <i class="fas fa-cloud-sun mr-4"></i>Weather Station Dashboard
      </h1>
      <p class="text-xl text-blue-100">Real-time indoor and outdoor monitoring</p>
      <div class="mt-4 flex items-center justify-center text-green-300">
        <span class="status-indicator status-online pulse"></span>
        <span class="text-lg">System Online - Last updated: <span id="lastUpdate">%LAST_UPDATED%</span></span>
      </div>
    </div>

    <!-- Main Grid Layout -->
    <div class="grid grid-cols-1 xl:grid-cols-2 gap-8 mb-8">
      
      <!-- Indoor Conditions Card -->
      <div class="bg-white/95 backdrop-blur-sm rounded-2xl p-8 shadow-2xl card-hover">
        <div class="flex items-center justify-between mb-6">
          <h2 class="text-3xl font-bold text-gray-800 flex items-center">
            <i class="fas fa-home text-green-600 mr-3 text-4xl"></i>
            Indoor Environment
          </h2>
          <div class="text-right">
            <div class="text-sm text-gray-500">Comfort Level</div>
            <div class="text-lg font-semibold text-green-600" id="indoorComfort">%INDOOR_COMFORT%</div>
          </div>
        </div>
        
        <div class="grid grid-cols-1 md:grid-cols-2 gap-8">
          <div class="text-center p-6 bg-gradient-to-br from-red-50 to-orange-50 rounded-xl">
            <i class="fas fa-thermometer-half text-red-500 text-5xl mb-4"></i>
            <div class="text-gray-700 text-xl mb-2">Temperature</div>
            <div class="temp-display text-red-600" id="indoorTemp">%INDOOR_TEMP%</div>
            <div class="text-2xl text-gray-600">°C</div>
          </div>
          
          <div class="text-center p-6 bg-gradient-to-br from-blue-50 to-cyan-50 rounded-xl">
            <i class="fas fa-tint text-blue-500 text-5xl mb-4"></i>
            <div class="text-gray-700 text-xl mb-2">Humidity</div>
            <div class="temp-display text-blue-600" id="indoorHumidity">%INDOOR_HUMIDITY%</div>
            <div class="text-2xl text-gray-600">%</div>
          </div>
        </div>
      </div>

      <!-- Outdoor Conditions Card -->
      <div class="bg-white/95 backdrop-blur-sm rounded-2xl p-8 shadow-2xl card-hover">
        <div class="flex items-center justify-between mb-6">
          <h2 class="text-3xl font-bold text-gray-800 flex items-center">
            <i class="fas fa-sun text-orange-500 mr-3 text-4xl"></i>
            Outdoor Conditions
          </h2>
          <div class="text-right">
            <div class="text-sm text-gray-500">Location</div>
            <div class="text-lg font-semibold text-orange-600">Skopje, MK</div>
          </div>
        </div>
        
        <div class="grid grid-cols-1 md:grid-cols-2 gap-8">
          <div class="text-center p-6 bg-gradient-to-br from-orange-50 to-yellow-50 rounded-xl">
            <i class="fas fa-thermometer-half text-orange-500 text-5xl mb-4"></i>
            <div class="text-gray-700 text-xl mb-2">Temperature</div>
            <div class="temp-display text-orange-600" id="outdoorTemp">%OUTDOOR_TEMP%</div>
            <div class="text-2xl text-gray-600">°C</div>
          </div>
          
          <div class="text-center p-6 bg-gradient-to-br from-blue-50 to-indigo-50 rounded-xl">
            <i class="fas fa-tint text-blue-500 text-5xl mb-4"></i>
            <div class="text-gray-700 text-xl mb-2">Humidity</div>
            <div class="temp-display text-blue-600" id="outdoorHumidity">%OUTDOOR_HUMIDITY%</div>
            <div class="text-2xl text-gray-600">%</div>
          </div>
        </div>
      </div>
    </div>

    <!-- Weather Description and Recommendation Row -->
    <div class="grid grid-cols-1 xl:grid-cols-2 gap-8">
      
      <!-- Weather Description Card -->
      <div class="bg-white/95 backdrop-blur-sm rounded-2xl p-8 shadow-2xl card-hover">
        <h2 class="text-3xl font-bold text-gray-800 flex items-center mb-6">
          <i class="fas fa-cloud text-blue-500 mr-3 text-4xl"></i>
          Current Weather
        </h2>
        <div class="text-center p-6 bg-gradient-to-br from-blue-50 to-purple-50 rounded-xl">
          <i class="fas fa-eye text-blue-400 text-5xl mb-4"></i>
          <div class="text-2xl font-semibold text-gray-800 capitalize" id="weatherDesc">%WEATHER%</div>
          <div class="text-lg text-gray-600 mt-2">Sky Conditions</div>
        </div>
      </div>

      <!-- Weather Recommendation Card -->
      <div class="bg-white/95 backdrop-blur-sm rounded-2xl p-8 shadow-2xl card-hover">
        <h2 class="text-3xl font-bold text-gray-800 flex items-center mb-6">
          <i class="fas fa-lightbulb text-yellow-500 mr-3 text-4xl"></i>
          Smart Recommendations
        </h2>
        <div class="p-6 bg-gradient-to-br from-yellow-50 to-green-50 rounded-xl border-l-4 border-green-500">
          <i class="fas fa-info-circle text-green-500 text-2xl mb-3"></i>
          <p class="text-xl text-gray-800 leading-relaxed" id="recommendation">%RECOMMENDATION%</p>
        </div>
      </div>
    </div>

    <!-- Temperature Difference Card -->
    <div class="mt-8">
      <div class="bg-white/95 backdrop-blur-sm rounded-2xl p-8 shadow-2xl card-hover">
        <h2 class="text-3xl font-bold text-gray-800 flex items-center mb-6">
          <i class="fas fa-exchange-alt text-purple-500 mr-3 text-4xl"></i>
          Environment Comparison
        </h2>
        <div class="grid grid-cols-1 md:grid-cols-3 gap-6">
          <div class="text-center p-6 bg-gradient-to-br from-purple-50 to-pink-50 rounded-xl">
            <i class="fas fa-thermometer-empty text-purple-500 text-4xl mb-3"></i>
            <div class="text-lg text-gray-700 mb-2">Temperature Difference</div>
            <div class="text-3xl font-bold text-purple-600" id="tempDiff">%TEMP_DIFF%°C</div>
          </div>
          <div class="text-center p-6 bg-gradient-to-br from-teal-50 to-cyan-50 rounded-xl">
            <i class="fas fa-water text-teal-500 text-4xl mb-3"></i>
            <div class="text-lg text-gray-700 mb-2">Humidity Difference</div>
            <div class="text-3xl font-bold text-teal-600" id="humidityDiff">%HUMIDITY_DIFF%%</div>
          </div>
          <div class="text-center p-6 bg-gradient-to-br from-green-50 to-emerald-50 rounded-xl">
            <i class="fas fa-cloud-upload-alt text-green-500 text-4xl mb-3"></i>
            <div class="text-lg text-gray-700 mb-2">ThingSpeak Status</div>
            <div class="text-2xl font-bold text-green-600" id="thingSpeakStatus">Connected</div>
          </div>
        </div>
      </div>
    </div>

    <!-- Footer -->
    <div class="text-center mt-8 text-blue-100">
      <p class="text-lg">
        <i class="fas fa-microchip mr-2"></i>
        Powered by ESP8266 | Auto-refresh every 10 seconds
      </p>
      <p class="text-md mt-2">
        <i class="fas fa-chart-line mr-2"></i>
        Data logged to ThingSpeak Channel: 2978168 | 
        <a href="https://thingspeak.com/channels/2978168" target="_blank" class="text-blue-200 hover:text-white underline">
          View Charts
        </a>
      </p>
    </div>
  </div>

  <script>
    // Auto-refresh data every 10 seconds
    setInterval(function() {
      fetch('/indoorTemp').then(r => r.text()).then(data => {
        document.getElementById('indoorTemp').textContent = parseFloat(data).toFixed(1);
      });
      fetch('/indoorHumidity').then(r => r.text()).then(data => {
        document.getElementById('indoorHumidity').textContent = parseFloat(data).toFixed(1);
      });
      fetch('/outdoorTemp').then(r => r.text()).then(data => {
        document.getElementById('outdoorTemp').textContent = parseFloat(data).toFixed(1);
      });
      fetch('/outdoorHumidity').then(r => r.text()).then(data => {
        document.getElementById('outdoorHumidity').textContent = parseFloat(data).toFixed(1);
      });
      fetch('/weather').then(r => r.text()).then(data => {
        document.getElementById('weatherDesc').textContent = data;
      });
      fetch('/recommendation').then(r => r.text()).then(data => {
        document.getElementById('recommendation').textContent = data;
      });
      
      // Update timestamp
      const now = new Date();
      document.getElementById('lastUpdate').textContent = now.toLocaleTimeString();
      
      // Calculate and update differences
      const indoorT = parseFloat(document.getElementById('indoorTemp').textContent);
      const outdoorT = parseFloat(document.getElementById('outdoorTemp').textContent);
      const indoorH = parseFloat(document.getElementById('indoorHumidity').textContent);
      const outdoorH = parseFloat(document.getElementById('outdoorHumidity').textContent);
      
      const tempDiff = (outdoorT - indoorT).toFixed(1);
      const humidityDiff = (outdoorH - indoorH).toFixed(1);
      
      document.getElementById('tempDiff').textContent = (tempDiff > 0 ? '+' : '') + tempDiff + '°C';
      document.getElementById('humidityDiff').textContent = (humidityDiff > 0 ? '+' : '') + humidityDiff + '%';
      
      // Update indoor status
      let status = "Optimal";
      if (indoorT < 18 || indoorT > 26) status = "Temperature Issue";
      else if (indoorH < 30 || indoorH > 60) status = "Humidity Issue";
      document.getElementById('indoorStatus').textContent = status;
      
    }, 10000);
  </script>
</body>
</html>
)rawliteral";

// Replaces placeholders
String processor(const String& var) {
  if (var == "INDOOR_TEMP") return String(indoorTemp, 1);
  if (var == "INDOOR_HUMIDITY") return String(indoorHumidity, 1);
  if (var == "OUTDOOR_TEMP") return String(outdoorTemp, 1);
  if (var == "OUTDOOR_HUMIDITY") return String(outdoorHumidity, 1);
  if (var == "WEATHER") return weatherDescription;
  if (var == "RECOMMENDATION") return weatherRecommendation;
  if (var == "LAST_UPDATED") return lastUpdated;
  if (var == "INDOOR_COMFORT") return getComfortLevel(indoorTemp, indoorHumidity);
  if (var == "TEMP_DIFF") return String(outdoorTemp - indoorTemp, 1);
  if (var == "HUMIDITY_DIFF") return String(outdoorHumidity - indoorHumidity, 1);
  if (var == "INDOOR_STATUS") {
    if (indoorTemp < 18 || indoorTemp > 26) return "Temperature Issue";
    if (indoorHumidity < 30 || indoorHumidity > 60) return "Humidity Issue";
    return "Optimal";
  }
  return String();
}

// API fetch for outdoor weather
void updateOutdoorWeather() {
  WiFiClient client;
  HTTPClient http;
  http.begin(client, weatherApiUrl);
  http.setTimeout(10000); // 10 second timeout
  int httpCode = http.GET();

  if (httpCode > 0) {
    String payload = http.getString();
    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, payload);
    
    if (!error) {
      outdoorTemp = doc["main"]["temp"].as<float>();
      outdoorHumidity = doc["main"]["humidity"].as<float>();
      weatherDescription = doc["weather"][0]["description"].as<String>();
    } else {
      weatherDescription = "JSON Parse Error";
    }
  } else {
    weatherDescription = "API Connection Error";
  }

  http.end();
}

// Send data to ThingSpeak
void sendToThingSpeak() {
  WiFiClient client;
  
  if (client.connect(thingSpeakServer.c_str(), 80)) {
    // Prepare the data string
    String postStr = "api_key=" + thingSpeakApiKey;
    postStr += "&field1=" + String(indoorTemp, 2);
    postStr += "&field2=" + String(indoorHumidity, 2);
    
    // Send HTTP POST request
    client.print("POST /update HTTP/1.1\n");
    client.print("Host: " + thingSpeakServer + "\n");
    client.print("Connection: close\n");
    client.print("X-THINGSPEAKAPIKEY: " + thingSpeakApiKey + "\n");
    client.print("Content-Type: application/x-www-form-urlencoded\n");
    client.print("Content-Length: ");
    client.print(postStr.length());
    client.print("\n\n");
    client.print(postStr);
    
    Serial.println("Data sent to ThingSpeak successfully!");
    Serial.println("Temperature: " + String(indoorTemp, 2) + "°C");
    Serial.println("Humidity: " + String(indoorHumidity, 2) + "%");
  } else {
    Serial.println("Failed to connect to ThingSpeak");
  }
  
  client.stop();
}

void setup() {
  Serial.begin(115200);
  dht.begin();

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("WiFi connected successfully!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // Initialize with first readings
  float newTemp = dht.readTemperature();
  float newHumidity = dht.readHumidity();
  if (!isnan(newTemp)) indoorTemp = newTemp;
  if (!isnan(newHumidity)) indoorHumidity = newHumidity;
  
  updateOutdoorWeather();
  updateWeatherRecommendation();

  // Server routes
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", index_html, processor);
  });
  
  server.on("/indoorTemp", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", String(indoorTemp, 1));
  });
  
  server.on("/indoorHumidity", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", String(indoorHumidity, 1));
  });
  
  server.on("/outdoorTemp", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", String(outdoorTemp, 1));
  });
  
  server.on("/outdoorHumidity", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", String(outdoorHumidity, 1));
  });
  
  server.on("/weather", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", weatherDescription);
  });
  
  server.on("/recommendation", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", weatherRecommendation);
  });

  // Additional API endpoints for enhanced features
  server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request) {
    String json = "{";
    json += "\"indoor_temp\":" + String(indoorTemp, 1) + ",";
    json += "\"indoor_humidity\":" + String(indoorHumidity, 1) + ",";
    json += "\"outdoor_temp\":" + String(outdoorTemp, 1) + ",";
    json += "\"outdoor_humidity\":" + String(outdoorHumidity, 1) + ",";
    json += "\"weather\":\"" + weatherDescription + "\",";
    json += "\"recommendation\":\"" + weatherRecommendation + "\",";
    json += "\"comfort\":\"" + getComfortLevel(indoorTemp, indoorHumidity) + "\",";
    json += "\"uptime\":" + String(millis()) + "";
    json += "}";
    request->send(200, "application/json", json);
  });

  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  unsigned long currentMillis = millis();
  
  // Regular sensor reading and display update (every 10 seconds)
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    // Read indoor sensors
    float newTemp = dht.readTemperature();
    float newHumidity = dht.readHumidity();

    if (!isnan(newTemp)) {
      indoorTemp = newTemp;
    } else {
      Serial.println("Failed to read temperature from DHT sensor!");
    }
    
    if (!isnan(newHumidity)) {
      indoorHumidity = newHumidity;
    } else {
      Serial.println("Failed to read humidity from DHT sensor!");
    }

    // Update outdoor weather and recommendations
    updateOutdoorWeather();
    updateWeatherRecommendation();

    // Print current readings to serial for debugging
    // Serial.println("=== Current Readings ===");
    // Serial.println("Indoor - Temp: " + String(indoorTemp, 1) + "°C, Humidity: " + String(indoorHumidity, 1) + "%");
    // Serial.println("Outdoor - Temp: " + String(outdoorTemp, 1) + "°C, Humidity: " + String(outdoorHumidity, 1) + "%");
    // Serial.println("Weather: " + weatherDescription);
    // Serial.println("Comfort: " + getComfortLevel(indoorTemp, indoorHumidity));
    // Serial.println("========================");
  }
  
  // ThingSpeak data upload (every 60 seconds - ThingSpeak rate limit)
  if (currentMillis - previousThingSpeakMillis >= thingSpeakInterval) {
    previousThingSpeakMillis = currentMillis;
    
    // Only send to ThingSpeak if we have valid sensor readings
    if (!isnan(indoorTemp) && !isnan(indoorHumidity) && indoorTemp != 0.0) {
      Serial.println("=== Sending to ThingSpeak ===");
      sendToThingSpeak();
      Serial.println("=============================");
    } else {
      Serial.println("Invalid sensor data - skipping ThingSpeak upload");
    }
  }
}