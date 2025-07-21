#include <ESP8266WiFi.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
 
#define SCREEN_WIDTH 128    // OLED display width, in pixels
#define SCREEN_HEIGHT 64    // OLED display height, in pixels
#define OLED_RESET -1       // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
 
int num_Measure = 128 ; // Set the number of measurements   
int pinSignal = A0; // pin connected to pin O module sound sensor  
long Sound_signal;    // Store the value read Sound Sensor   
long sum = 0 ; // Store the total value of n measurements   
long level = 0 ; // Store the average value   
int soundlow = 40;
int soundmedium = 500;
int error = 33;
 
String apiKey = "V78PJ4XYQEL5VFFJ"; // Enter your Write API key from ThingSpeak
const char *ssid = "Intelory WiFi";     // replace with your wifi ssid and wpa2 key
const char *pass = "LuTa6OF0@INT";
const char* server = "api.thingspeak.com";
 
WiFiClient client;
 
void setup ()  
{   
  pinMode (pinSignal, INPUT); // Set the signal pin as input   
  Serial.begin (115200);  
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C); //initialize with the I2C addr 0x3C (128x64)
  display.clearDisplay();
  delay(10);
 
  Serial.println("Connecting to ");
  Serial.println(ssid);
  
  display.clearDisplay();
  display.setCursor(0,0);  
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.println("Connecting to ");
  display.setTextSize(2);
  display.print(ssid);
  display.display();
  
  WiFi.begin(ssid, pass);
 
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1000);
    Serial.print(".");
  }
    Serial.println("");
    Serial.println("WiFi connected");
    
    display.clearDisplay();
    display.setCursor(0,0);  
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.print("WiFi connected");
    display.display();
    delay(4000);
}  
 
   
void loop ()  
{ 
 
  // Performs 128 signal readings   
  for ( int i = 0 ; i <num_Measure; i ++)  
  {  
   Sound_signal = analogRead (pinSignal);  
    sum =sum + Sound_signal;  
  }  
 
  level = sum / num_Measure; // Calculate the average value  
   
  Serial.print("Sound Level: ");
  Serial.println (level-error);  
 
    display.clearDisplay();
    display.setCursor(0,0);  //oled display
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.println("--- Decibelmeter ---"); 
 
    
 
  if( (level-error) < soundlow)
  {
 
    Serial.print("Intensity= Low");
    display.setCursor(0,20);  //oled display
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.print("Sound Level: ");
    display.println(level-error);
 
    display.setCursor(0,40);
    display.print("Intensity: LOW");
    
    display.display();
//    {
//      if( (level-error) < 0)
//      Serial.print("Intensity= Low");
//      display.setCursor(0,20);  //oled display
//      display.setTextSize(1);
//      display.setTextColor(WHITE);
//      display.print("Sound Level: 0");
//
//      display.setCursor(0,40);
//      display.print("Intensity: LOW");
//    
//      display.display();
//    }
 
  }
  if( ( (level-error) > soundlow ) && ( (level-error) < soundmedium )  )
  {
    
    Serial.print("Intensity=Medium"); 
    display.setCursor(0,20);  //oled display
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.print("Sound Level: ");
    display.println(level-error);
 
    display.setCursor(0,40);
    display.print("Intensity: MEDIUM");
    display.display();
      
  }
  if( (level-error) > soundmedium )
  {
 
    Serial.print("Intensity= High");   
    display.setCursor(0,20);  //oled display
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.print("Sound Level: ");
    display.println(level-error);
 
    display.setCursor(0,40);
    display.print("Intensity: HIGH");
    display.display();
     
  }
  sum = 0 ; // Reset the sum of the measurement values  
  delay(200);
 
  if (client.connect(server, 80)) // "184.106.153.149" or api.thingspeak.com
  {
    String postStr = apiKey;
    postStr += "&field1=";
    postStr += String(level-error);
    postStr += "r\n";
    
    client.print("POST /update HTTP/1.1\n");
    client.print("Host: api.thingspeak.com\n");
    client.print("Connection: close\n");
    client.print("X-THINGSPEAKAPIKEY: " + apiKey + "\n");
    client.print("Content-Type: application/x-www-form-urlencoded\n");
    client.print("Content-Length: ");
    client.print(postStr.length());
    client.print("\n\n");
    client.print(postStr);
   
  }
    client.stop();
 
    //delay(15000);      // thingspeak needs minimum 15 sec delay between updates.
}