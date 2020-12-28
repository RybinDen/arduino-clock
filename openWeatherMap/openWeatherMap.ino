#include <ArduinoJson.h>
#include <SPI.h>
#include <Ethernet.h>
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
IPAddress ip(192, 168, 1, 8);
EthernetClient client;
#include "DHT.h"
DHT dht;
unsigned long previousMillis = 0;        // will store last time LED was updated
const long interval = 5000;           // interval at which to blink (milliseconds)
void setup() {
  dht.setup(15); // data pin 2
  Serial.begin(9600);
  while (!Serial) continue;
  if (!Ethernet.begin(mac)) {
    Serial.println(F("Failed to configure Ethernet"));
    Ethernet.begin(mac, ip);
  }
  delay(1000);
  Serial.println(F("Connecting..."));
  client.setTimeout(10000);
  if (client.connect("api.openweathermap.org", 80)) {
    Serial.println(F("Connected!"));
    client.println(F("GET /data/2.5/weather?q=Krapivinskiy,RU&units=metric&appid=openweathermap_token HTTP/1.1")); // указать свой город и ключ
    client.println(F("Host: api.openweathermap.org"));
    client.println(F("Connection: close"));
    if (client.println() == 0) Serial.println(F("Failed to send request"));
  } else {
    Serial.println(F("Connection failed"));
  }
  /* Check HTTP status
    char status[32] = {0};
    client.readBytesUntil('\r', status, sizeof(status));
    // It should be "HTTP/1.0 200 OK" or "HTTP/1.1 200 OK"
    if (strcmp(status + 9, "200 OK") != 0) {
    Serial.print(F("Unexpected response: "));
    Serial.println(status);
    return;
    }
  */
  // Skip HTTP headers
  char endOfHeaders[] = "\r\n\r\n";
  if (!client.find(endOfHeaders)) {
    Serial.println(F("Invalid response"));
    return;
  }
  // Allocate the JSON document
  // Use arduinojson.org/v6/assistant to compute the capacity.
  const size_t capacity = JSON_ARRAY_SIZE(1) + JSON_OBJECT_SIZE(1) + 2 * JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(4) + JSON_OBJECT_SIZE(5) + JSON_OBJECT_SIZE(6) + JSON_OBJECT_SIZE(13) + 280;
  DynamicJsonDocument doc(capacity);
  // Parse JSON object
  DeserializationError error = deserializeJson(doc, client);
  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.c_str());
    return;
  }
  // Extract values
  Serial.println(F("Response:"));
  //Serial.println(doc["weather"].as<char*>());
  Serial.println(doc["main"]["temp"].as<int8_t>());
  Serial.println(doc["main"]["pressure"].as<int>() * 0.75, 0);
  Serial.println(doc["main"]["humidity"].as<int8_t>());
  Serial.println(doc["wind"]["speed"].as<int8_t>());
  Serial.println(doc["wind"]["deg"].as<int16_t>());
  convertDateTime(doc["dt"].as<uint32_t>());
  // Disconnect
  client.stop();
}
void convertDateTime(uint32_t datetime) {
  Serial.print(F("The UTC time is "));       // UTC is the time at Greenwich Meridian (GMT)
  Serial.print((datetime % 86400L) / 3600); // print the hour (86400 equals secs per day)
  Serial.print(F(": "));
  Serial.println((datetime % 3600) / 60); // print the minute (3600 equals secs per minute)
  //Serial.println(datetime % 60); // print the second
}

void dhtInfo() {
  delay(dht.getMinimumSamplingPeriod());
  float humidity = dht.getHumidity();
  float temperature = dht.getTemperature();
  Serial.print(dht.getStatusString());
  Serial.print("\t");
  Serial.print(temperature, 0);
  Serial.print("\t\t");
  Serial.println(humidity, 0);
}
void loop() {
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    dhtInfo();
    previousMillis = currentMillis;
  }
}
