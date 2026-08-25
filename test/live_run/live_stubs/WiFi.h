#ifndef MLB_LIVE_STUB_WIFI_H
#define MLB_LIVE_STUB_WIFI_H

// Stand-in for ESP32's <WiFi.h>, for test/live_run only (see Arduino.h in
// this directory). Your desktop already has a network connection, so
// this always reports connected -- there's no WiFi radio to actually
// drive here, and MLBDataSource::httpGetJson() only cares about
// WiFi.status() as a gate before it makes the real HTTP call.

#include "Arduino.h"

#define WL_CONNECTED 3
#define WL_DISCONNECTED 0
#define WIFI_STA 1

class WiFiClass
{
  public:
    int status() { return WL_CONNECTED; }
    void mode(int) {}
    bool begin(const char *, const char * = nullptr) { return true; }
    void disconnect(bool = false) {}
};

inline WiFiClass WiFi;

#endif // MLB_LIVE_STUB_WIFI_H
