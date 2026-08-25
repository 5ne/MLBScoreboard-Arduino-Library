#ifndef MLB_STUB_ESP_SLEEP_H
#define MLB_STUB_ESP_SLEEP_H

// Minimal stand-in for ESP-IDF's <esp_sleep.h>. Deliberately NOT marked
// noreturn/never actually sleeps -- this build-check only links the
// example sketches, it never executes setup()/loop(), so the real
// MLBScoreboard::sleepUntilNextPoll()'s `while(true){}` after this call
// is never reached. See Arduino.h in this directory for the full
// rationale.

#include <cstdint>

inline int esp_sleep_enable_timer_wakeup(uint64_t) { return 0; }
inline void esp_deep_sleep_start() {}

#endif // MLB_STUB_ESP_SLEEP_H
