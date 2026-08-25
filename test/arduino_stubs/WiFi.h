#ifndef MLB_STUB_WIFI_H
#define MLB_STUB_WIFI_H

// Minimal stand-in for ESP32's <WiFi.h> -- see Arduino.h in this
// directory for why this stub exists and what it's for (a build/link
// check, never actually run).

#include "Arduino.h"

#define WL_CONNECTED 3
#define WL_DISCONNECTED 0
#define WIFI_STA 1

class WiFiClass
{
  public:
    // Always reports "not connected" -- this build-check never opens a
    // real socket, so every network-touching code path in
    // MLBDataSource::httpGetJson() short-circuits at its
    // `WiFi.status() != WL_CONNECTED` guard if the binary is ever run.
    // Compiling and linking is what this check verifies, not live
    // network behavior.
    int status() { return WL_DISCONNECTED; }
    void mode(int) {}
    bool begin(const char * /*ssid*/, const char * /*password*/ = nullptr) { return false; }
    void disconnect(bool = false) {}
};

inline WiFiClass WiFi;

#endif // MLB_STUB_WIFI_H
