#ifndef MLB_LIVE_STUB_ARDUINO_H
#define MLB_LIVE_STUB_ARDUINO_H

// Stand-in for the Arduino/ESP32 core -- for test/live_run, NOT for
// test/check_examples_compile.sh (that one's stubs, one directory over in
// test/arduino_stubs/, are deliberately inert: build/link only, never
// run). This one is meant to be run: Serial.printf really prints, and
// millis() is a real clock. See test/live_run/README.md for why this
// exists as a second, separate set of stubs instead of reusing the other
// one.

#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <string>
#include <chrono>
#include <thread>
#include <ctime>

// -- String -----------------------------------------------------------
class String
{
  public:
    String() {}
    String(const char *s) : _s(s ? s : "") {}
    String(const std::string &s) : _s(s) {}
    const char *c_str() const { return _s.c_str(); }
    size_t length() const { return _s.length(); }
    // Used for real (not just to satisfy the compiler) -- ArduinoJson's
    // ArduinoStringWriter.hpp calls this when serializing into a String,
    // pulled in now that ARDUINOJSON_ENABLE_ARDUINO_STRING is on.
    bool concat(const char *s)
    {
        _s += (s ? s : "");
        return true;
    }

  private:
    std::string _s;
};

// -- Print / Stream / Printable ----------------------------------------
// Same shape as test/arduino_stubs/Arduino.h -- see that file's comments
// for why each piece is here (ArduinoJson's Arduino-Stream support needs
// all three to type-check).
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

class Printable
{
  public:
    virtual ~Printable() {}
    virtual size_t printTo(Print &p) const = 0;
};

// -- Serial -- actually prints, unlike the build-check stub -------------
class HardwareSerial : public Stream
{
  public:
    void begin(unsigned long) {}

    void printf(const char *fmt, ...)
    {
        va_list args;
        va_start(args, fmt);
        vprintf(fmt, args);
        va_end(args);
        fflush(stdout);
    }

    void print(const char *s) { fputs(s, stdout); }
    void print(int v) { printf("%d", v); }
    void println(const char *s)
    {
        fputs(s, stdout);
        fputc('\n', stdout);
    }
    void println() { fputc('\n', stdout); }
};

inline HardwareSerial Serial;

// -- Real clock, real delay ---------------------------------------------
// connectWifi()'s 15s timeout loop never actually runs here (WiFiClass::
// status() below always reports connected), but millis()/delay() are
// real in case anything else in the call path depends on them advancing.
inline unsigned long millis()
{
    using namespace std::chrono;
    static const auto start = steady_clock::now();
    return (unsigned long)duration_cast<milliseconds>(steady_clock::now() - start).count();
}

inline void delay(unsigned long ms)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

// -- Time sync (ESP32 core: esp32-hal-time.h) -------------------------------
// MLBScoreboard::syncTimeIfNeeded() calls these to recover from a
// never-synced clock on real hardware; the desktop already has a
// correct system clock (and WiFiClass::status() below always reports
// connected, which is what gates whether that call happens at all), so
// these are no-ops/pass-through-to-the-real-clock rather than doing an
// actual NTP round trip.
inline void configTime(long, int, const char *, const char * = nullptr, const char * = nullptr) {}

inline bool getLocalTime(struct tm *info, uint32_t = 5000)
{
    time_t now;
    time(&now);
    return localtime_r(&now, info) != nullptr;
}

#endif // MLB_LIVE_STUB_ARDUINO_H
