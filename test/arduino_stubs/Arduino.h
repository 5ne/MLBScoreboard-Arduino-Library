#ifndef MLB_STUB_ARDUINO_H
#define MLB_STUB_ARDUINO_H

// Minimal stand-in for the Arduino/ESP32 core -- just enough of the API
// surface for MLBScoreboard's real source files (and the example .ino
// sketches, unmodified) to compile and LINK on a desktop compiler.
//
// This is a build/link check, not a hardware simulator: it exists to
// catch exactly the class of bug a real Arduino IDE compile catches --
// a method declared but never defined (undefined reference at link
// time), or called but never declared (compile error) -- without needing
// the real ESP32 toolchain or the Inkplate/WiFi/HTTPClient libraries.
// See test/README.md for what this does and doesn't cover, and
// test/check_examples_compile.sh for how it's invoked.
//
// Nothing in here is ever *executed* by the test suite (the resulting
// binary is built, not run), so bodies are deliberately trivial no-ops
// except where a real timeout/retry loop in the production code needs
// the fake clock below to terminate instead of spinning forever if it
// ever were run.

#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <string>
#include <ctime>

// -- String ---------------------------------------------------------------
// Arduino's String class, reduced to what MLBDataSource actually uses:
// construct from a C string, read back via c_str()/length().
class String
{
  public:
    String() {}
    String(const char *s) : _s(s ? s : "") {}
    String(const std::string &s) : _s(s) {}
    const char *c_str() const { return _s.c_str(); }
    size_t length() const { return _s.length(); }
    // Needed for ArduinoJson's ArduinoStringWriter.hpp, pulled in now
    // that ARDUINOJSON_ENABLE_ARDUINO_STRING is on (see
    // check_examples_compile.sh) -- never actually exercised, since this
    // build-check never calls serializeJson().
    bool concat(const char *s)
    {
        _s += (s ? s : "");
        return true;
    }

  private:
    std::string _s;
};

// -- Print / Stream ---------------------------------------------------------
// Real Arduino hierarchy: Stream derives from Print; HardwareSerial and
// WiFiClient both derive from Stream. ArduinoJson's Arduino-Stream reader
// support (enabled whenever ARDUINO is defined) is a template partial
// specialization keyed on `is_base_of<Stream, TSource>` -- HTTPClient's
// getStream() needs to return something derived from this Stream for
// deserializeJson(doc, http.getStream(), ...) to type-check, exactly as
// it does against the real core.
class Print
{
  public:
    virtual ~Print() {}
    virtual size_t write(uint8_t) { return 0; }
    virtual size_t write(const uint8_t *buffer, size_t size)
    {
        (void)buffer;
        return size;
    }
};

class Stream : public Print
{
  public:
    virtual int available() { return 0; }
    virtual int read() { return -1; }
    virtual size_t readBytes(char *buffer, size_t length)
    {
        (void)buffer;
        (void)length;
        return 0;
    }
};

// Real Arduino's Printable interface. ArduinoJson's Arduino-Stream
// support (ARDUINOJSON_ENABLE_ARDUINO_STREAM, on whenever ARDUINO is
// defined) declares a free `convertToJson(const Printable&, ...)`
// overload in the same header as the Stream reader we actually need;
// since it's an ordinary (non-template) inline function, its body must
// type-check just from the header being included, whether or not our
// code ever constructs a Printable. We never do -- this type only needs
// to exist and match the real interface shape.
class Printable
{
  public:
    virtual ~Printable() {}
    virtual size_t printTo(Print &p) const = 0;
};

class HardwareSerial : public Stream
{
  public:
    void begin(unsigned long) {}
    void printf(const char *fmt, ...)
    {
        // No-op: this build-check never executes the resulting binary,
        // it only needs Serial.printf(...) (used by the MLB_DEBUG/
        // MLB_INFO/MLB_ERROR logging macros) to compile and link.
        (void)fmt;
    }
    void print(const char *) {}
    void print(int) {}
    void println(const char *) {}
    void println() {}
};

inline HardwareSerial Serial;

// -- Fake, monotonic-only clock --------------------------------------------
// delay(ms) advances the fake clock instead of really sleeping, so any
// bounded retry/timeout loop in the real source (e.g.
// MLBScoreboard::connectWifi()'s 15s WiFi-connect timeout) would
// terminate instantly rather than hang, if this build were ever run
// instead of just linked.
inline unsigned long &mlbStubFakeMillis()
{
    static unsigned long ms = 0;
    return ms;
}

inline unsigned long millis() { return mlbStubFakeMillis(); }
inline void delay(unsigned long ms) { mlbStubFakeMillis() += ms; }

// -- Time sync (ESP32 core: esp32-hal-time.h) -------------------------------
// Real signatures, no-op/trivial bodies -- see the file-level comment
// above for why a real implementation isn't needed here.
inline void configTime(long gmtOffset_sec, int daylightOffset_sec, const char *server1, const char *server2 = nullptr,
                        const char *server3 = nullptr)
{
    (void)gmtOffset_sec;
    (void)daylightOffset_sec;
    (void)server1;
    (void)server2;
    (void)server3;
}

inline bool getLocalTime(struct tm *info, uint32_t ms = 5000)
{
    (void)ms;
    time_t now;
    time(&now);
    return localtime_r(&now, info) != nullptr;
}

#endif // MLB_STUB_ARDUINO_H
