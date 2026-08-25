#ifndef MLB_STUB_HTTPCLIENT_H
#define MLB_STUB_HTTPCLIENT_H

// Minimal stand-in for ESP32's <HTTPClient.h> -- see Arduino.h in this
// directory for why this stub exists (a build/link check, never
// actually run).

#include "Arduino.h"
#include "WiFi.h"

#define HTTP_CODE_OK 200

// A Stream-derived body so deserializeJson(doc, http.getStream(), ...)
// type-checks exactly the way it does against the real
// WiFiClient-backed stream.
class HttpStreamStub : public Stream
{
};

class HTTPClient
{
  public:
    void setTimeout(uint32_t) {}
    bool begin(const String &) { return true; }
    // Always fails: this stub never makes a real request. See
    // WiFiClass::status() for why -- httpGetJson() bails out before
    // ever reaching GET() if this binary is run rather than just built.
    int GET() { return -1; }
    Stream &getStream() { return _stream; }
    String getString() { return String(""); }
    void end() {}

  private:
    HttpStreamStub _stream;
};

#endif // MLB_STUB_HTTPCLIENT_H
