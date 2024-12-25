// WiFi Config Library
// 2020-05-23,2024-11-16  T. Nakagawa
// This library allows users to access SoftAP (SSID=ESP32, password=12345678) and http://192.168.0.1/ for registering key/value pairs.

#ifndef WIFICONFIG_H_
#define WIFICONFIG_H_

#include <Preferences.h>
#include <WiFi.h>

class WiFiConfig {
public:
  static void configure(const char *name) {
    if (Serial) Serial.println("WiFiConfig started.");
    Preferences pref;
    pref.begin(name, false);
    if (Serial) Serial.println("Free entries: " + String(pref.freeEntries()));
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(IPAddress(192, 168, 0, 1), IPAddress(192, 168, 0, 1), IPAddress(255, 255, 255, 0));
    WiFi.softAP("ESP32", "12345678");
    WiFiServer server(80);
    server.begin();
    bool quit = false;
    while (!quit) {
      WiFiClient client = server.accept();
      if (!client) continue;
      const String line = client.readStringUntil('\n');
      String message;
      if (line.startsWith("GET /?")) {
        String key;
        String val;
        String buf = line.substring(6);
        int pos = buf.indexOf(" ");
        if (pos < 0) pos = 0;
        buf = buf.substring(0, pos);
        buf.concat("&");
        while (buf.length()) {
          int pos = buf.indexOf("&");
          const String param = buf.substring(0, pos);
          buf = buf.substring(pos + 1);
          pos = param.indexOf("=");
          if (pos < 0) continue;
          if (param.substring(0, pos) == "key") key = urlDecode(param.substring(pos + 1));
          else if (param.substring(0, pos) == "val") val = urlDecode(param.substring(pos + 1));
        }
        key.trim();
        val.trim();
        if (Serial) Serial.println("key=" + key + ", val=" + val);
        if (key.length()) {
          pref.putString(key.c_str(), val);
          if (pref.getString(key.c_str()) == val) {
            message = "Succeeded to update: " + key;
          } else {
            message = "Failed to update: " + key;
          }
        } else {
          quit = true;
          message = "Quitted.";
        }
      }

      client.println("<!DOCTYPE html>");
      client.println("<html>");
      client.println("<head><title>Configuration</title></head>");
      client.println("<body>");
      client.println("<h1>Configuration (Submit an empty key to quit)</h1>");
      client.println("<form action=\"/\" method=\"get\">Key: <input type=\"text\" name=\"key\" size=\"10\"> Value: <input type=\"text\" name=\"val\" size=\"20\"> <input type=\"submit\"></form>");
      client.println("<p>" + message + "</p>");
      client.println("</body>");
      client.println("</html>");
      client.stop();
    }
    server.end();
    WiFi.disconnect(true);
    pref.end();
    if (Serial) Serial.println("Config finished.");
  }

private:
  static String urlDecode(const String &str) {
    String result;
    for (int i = 0; i < str.length(); i++) {
      const char c = str[i];
      if (c == '+') {
        result.concat(" ");
      } else if (c == '%' && i + 2 < str.length()) {
        const char c0 = str[++i];
        const char c1 = str[++i];
        unsigned char d = 0;
        d += (c0 <= '9') ? c0 - '0' : (c0 <= 'F') ? c0 - 'A' + 10 : c0 - 'a' + 10;
        d <<= 4;
        d += (c1 <= '9') ? c1 - '0' : (c1 <= 'F') ? c1 - 'A' + 10 : c1 - 'a' + 10;
        result.concat((char)d);
      } else {
        result.concat(c);
      }
    }
    return result;
  }
};

#endif
