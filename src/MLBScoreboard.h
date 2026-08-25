#ifndef MLB_SCOREBOARD_H
#define MLB_SCOREBOARD_H

#include <Inkplate.h>
#include "MLBGame.h"
#include "MLBDataSource.h"
#include "MLBLogging.h"
#include "renderers/ScoreRenderer.h"

// How aggressively to poll statsapi.mlb.com. These are deliberately
// conservative defaults -- the API has no published rate limit, so this
// library errs on the side of being a polite, occasional client rather
// than a fast one. Tune down pollLiveMs if you want snappier live
// updates and are comfortable polling more often.
struct ScoreboardConfig
{
    const char *wifiSsid = nullptr;
    const char *wifiPassword = nullptr;

    static const int kMaxTeams = 8;
    const char *teamAbbreviations[kMaxTeams] = {nullptr};
    int teamCount = 0;
    int favoriteTeamIndex = -1; // index into teamAbbreviations, or -1

    unsigned long pollPreviewMs = 15UL * 60UL * 1000UL; // 15 min pregame
    unsigned long pollLiveMs = 60UL * 1000UL;            // 60 sec during live games
    unsigned long pollFinalMs = 6UL * 60UL * 60UL * 1000UL; // 6 hr once final (basically "stop")

    // If true, calls esp_deep_sleep() at the end of tick() using the
    // computed next-poll interval instead of returning. Only meaningful
    // on battery-powered installs -- leave false for USB-powered boards
    // running a normal loop()/delay() cycle.
    bool useDeepSleep = false;
};

// Ties MLBDataSource (network) and a ScoreRenderer (drawing) together
// with a WiFi connect and an adaptive poll/sleep schedule. This is the
// class most sketches will use directly; reach for MLBDataSource /
// ScoreRenderer separately only if you need custom orchestration.
class MLBScoreboard
{
  public:
    MLBScoreboard(Inkplate &display, ScoreRenderer &renderer);

    void begin(const ScoreboardConfig &config);

    // Set timezone offset in minutes from UTC for correct local time display.
    // Examples: PDT = -420 (UTC-7), MDT = -360 (UTC-6), CDT = -300 (UTC-5), EDT = -240 (UTC-4)
    void setTimezoneOffsetMinutes(int offsetMinutes);

    // Enable or disable debug logging to Serial (Arduino IDE Serial Monitor).
    // Info and error messages are always logged; debug messages only when enabled.
    void setDebugLogging(bool enabled);

    // Connects WiFi (if not already connected), fetches the latest state
    // for every configured team, renders, and flushes to the display.
    // Returns false if WiFi or every fetch failed -- in that case the
    // last-known-good games[] are re-rendered with a "stale" indicator
    // rather than left blank, since e-paper holds whatever was last
    // drawn otherwise anyway.
    bool tick();

    // Computes how long to wait before the next tick(), based on the
    // most "urgent" state across all tracked games (any live game wins,
    // else the earliest preview, else the final interval).
    unsigned long nextPollIntervalMs() const;

    // Convenience for battery installs: calls nextPollIntervalMs() and
    // puts the ESP32 into deep sleep for that long. Does not return.
    [[noreturn]] void sleepUntilNextPoll();

  private:
    Inkplate &_display;
    ScoreRenderer &_renderer;
    MLBDataSource _dataSource;
    ScoreboardConfig _config;

    MLBGame _games[ScoreboardConfig::kMaxTeams];

    bool connectWifi();
};

#endif // MLB_SCOREBOARD_H
