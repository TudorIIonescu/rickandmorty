#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "BluetoothSerial.h"

// API endpoints
const char* URL1 = "http://proiectia.bogdanflorea.ro/api/rick-and-morty/characters";
const char* URL2 = "http://proiectia.bogdanflorea.ro/api/rick-and-morty/character?id=";

// Bluetooth Classic device name and team ID
#define BT_NAME   "RickMorty"
const char* TEAM_ID = "A26";

BluetoothSerial SerialBT;

// Helper to connect to WiFi
bool connectWiFi(const String& ssid, const String& password) {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  WiFi.begin(ssid.c_str(), password.c_str());
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - start > 10000) return false;
    delay(500);
  }
  return true;
}

// Main command dispatcher
void handleCommand(const String& jsonIn) {
  StaticJsonDocument<8192> req;
  if (deserializeJson(req, jsonIn) != DeserializationError::Ok) {
    SerialBT.println("{\"error\":\"invalid JSON\"}");
    return;
  }
  const char* action = req["action"];
  StaticJsonDocument<16384> resp;
  String out;

  if (strcmp(action, "getNetworks") == 0) {
    int n = WiFi.scanNetworks();
    for (int i = 0; i < n; i++) {
      resp.clear();
      resp["ssid"]       = WiFi.SSID(i);
      resp["strength"]   = WiFi.RSSI(i);
      resp["encryption"] = WiFi.encryptionType(i);
      resp["teamId"]     = TEAM_ID;
      serializeJson(resp, out);
      SerialBT.println(out);
      delay(50);
    }
  }
  else if (strcmp(action, "connect") == 0) {
    String ssid     = req["ssid"].as<String>();
    String password = req["password"].as<String>();
    bool ok = connectWiFi(ssid, password);
    resp.clear();
    resp["teamId"]    = TEAM_ID;
    resp["ssid"]      = ssid;
    resp["connected"] = ok;
    serializeJson(resp, out);
    SerialBT.println(out);
  }
  else if (strcmp(action, "getData") == 0) {
    HTTPClient http;
    http.begin(URL1);
    http.setConnectTimeout(15000);
    http.setTimeout(15000);
    int code = http.GET();
    if (code != HTTP_CODE_OK) {
      resp.clear();
      resp["error"] = String("HTTP ") + code;
      serializeJson(resp, out);
      SerialBT.println(out);
    } else {
      String body = http.getString();
      StaticJsonDocument<16384> listDoc;
      if (deserializeJson(listDoc, body) == DeserializationError::Ok) {
        for (JsonVariant v : listDoc.as<JsonArray>()) {
          resp.clear();
          long cid = v["id"].as<long>();
          const char* cname = v["name"]  | "";
          const char* cimg   = v["img_url"] | "";
          resp["id"]      = cid;
          resp["name"]    = cname;
          resp["image"]   = cimg;
          resp["teamId"]  = TEAM_ID;
          serializeJson(resp, out);
          SerialBT.println(out);
          delay(50);
        }
      } else {
        SerialBT.println("{\"error\":\"JSON parse failed\"}");
      }
    }
    http.end();
  }
  else if (strcmp(action, "getDetails") == 0) {
    String id = req["id"].as<String>();
    HTTPClient http;
    http.begin(String(URL2) + id);
    http.setConnectTimeout(15000);
    http.setTimeout(15000);
    int code = http.GET();
    if (code != HTTP_CODE_OK) {
      resp.clear();
      resp["error"] = String("HTTP ") + code;
      serializeJson(resp, out);
      SerialBT.println(out);
    } else {
      String body = http.getString();
      StaticJsonDocument<8192> detailDoc;
      if (deserializeJson(detailDoc, body) == DeserializationError::Ok) {
        // Build description sub-object
        StaticJsonDocument<2048> descDoc;
        descDoc["status"]  = detailDoc["status"];
        descDoc["species"] = detailDoc["species"];
        descDoc["type"]    = detailDoc["type"];
        descDoc["gender"]  = detailDoc["gender"];
        {
          JsonObject src = detailDoc["origin"].as<JsonObject>();
          JsonObject dst = descDoc.createNestedObject("origin");
          dst["name"] = src["name"];
          dst["url"]  = src["url"];
        }
        {
          JsonObject src = detailDoc["location"].as<JsonObject>();
          JsonObject dst = descDoc.createNestedObject("location");
          dst["name"] = src["name"];
          dst["url"]  = src["url"];
        }
        {
          JsonArray src = detailDoc["episode"].as<JsonArray>();
          JsonArray dst = descDoc.createNestedArray("episode");
          for (const char* ep : src) dst.add(ep);
        }
        String description;
        serializeJson(descDoc, description);

        // Final response
        resp.clear();
        resp["id"]          = detailDoc["id"].as<int>();
        resp["name"]        = detailDoc["name"].as<const char*>();
        resp["image"]       = detailDoc["img_url"].as<const char*>();
        resp["description"] = description;
        resp["teamId"]      = TEAM_ID;
        serializeJson(resp, out);
        SerialBT.println(out);
      } else {
        SerialBT.println("{\"error\":\"JSON parse failed\"}");
      }
    }
    http.end();
  }
  else {
    SerialBT.println("{\"error\":\"unknown action\"}");
  }
}

void setup() {
  Serial.begin(115200);
  if (!SerialBT.begin(BT_NAME)) {
    Serial.println("Error initializing Bluetooth");
  } else {
    Serial.printf("Bluetooth started as \"%s\"\n", BT_NAME);
  }
}

void loop() {
  static String buffer;
  while (SerialBT.available()) {
    char c = SerialBT.read();
    buffer += c;
    if (c == '\n') {
      buffer.trim();
      if (buffer.length()) handleCommand(buffer);
      buffer = "";
    }
  }
}
