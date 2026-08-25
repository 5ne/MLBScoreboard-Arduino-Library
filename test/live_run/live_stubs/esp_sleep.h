#ifndef MLB_LIVE_STUB_ESP_SLEEP_H
#define MLB_LIVE_STUB_ESP_SLEEP_H

// Stand-in for ESP-IDF's <esp_sleep.h>, for test/live_run only (see
// Arduino.h in this directory). Needed only so MLBScoreboard.cpp (which
// calls these in sleepUntilNextPoll()) links -- run_live.cpp deliberately
// never calls sleepUntilNextPoll() itself (it would hang: the real
// method loops forever after esp_deep_sleep_start(), which makes sense
// on a device that's about to lose power to wake later, not on your
// desktop).

#include <cstdint>

inline int esp_sleep_enable_timer_wakeup(uint64_t) { return 0; }
inline void esp_deep_sleep_start() {}

#endif // MLB_LIVE_STUB_ESP_SLEEP_H
