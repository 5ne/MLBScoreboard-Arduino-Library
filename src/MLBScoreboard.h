#ifndef MLB_SCOREBOARD_H
#define MLB_SCOREBOARD_H

#include <Arduino.h>
#include "MLBGame.h"
#include "MLBDataSource.h"

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

// Fetches and tracks MLB game state for a set of teams on an adaptive
// poll/sleep schedule. This class's only job is turning "team
// abbreviations + a poll schedule" into an array of MLBGame structs that
// the calling application can read via games() -- it does NOT touch a
// display and has no dependency on Inkplate or any other rendering
// library. That keeps it usable by anything that wants structured score
// data (an e-paper renderer, a serial log, an MQTT publisher, a unit
// test) without dragging in display code it doesn't need.
//
// To put scores on an Inkplate board, pair this class with a
// ScoreRenderer in your sketch: call tick() to refresh games(), then pass
// games() / teamCount() / favoriteTeamIndex() to a renderer's render(),
// then flush the display yourself. See examples/.
class MLBScoreboard
{
  public:
    MLBScoreboard() = default;

    void begin(const ScoreboardConfig &config);

    // Connects WiFi (if not already connected) and fetches the latest
    // state for every configured team into the internal games() array.
    // Returns false if WiFi or every fetch failed -- in that case the
    // last-known-good games() are left in place with isStale set, rather
    // than cleared, so the caller can still show/report the last good
    // state instead of nothing at all.
    bool tick();

    // The current tracked game state: one entry per configured team, up
    // to teamCount() entries are meaningful. Owned by this object and
    // updated in place by tick() -- copy out anything you need to keep
    // past the next tick().
    const MLBGame *games() const { return _games; }
    int teamCount() const { return _config.teamCount; }
    int favoriteTeamIndex() const { return _config.favoriteTeamIndex; }

    // Computes how long to wait before the next tick(), based on the
    // most "urgent" state across all tracked games (any live game wins,
    // else the earliest preview, else the final interval).
    unsigned long nextPollIntervalMs() const;

    // Convenience for battery installs: calls nextPollIntervalMs() and
    // puts the ESP32 into deep sleep for that long. Does not return.
    [[noreturn]] void sleepUntilNextPoll();

  private:
    MLBDataSource _dataSource;
    ScoreboardConfig _config;

    MLBGame _games[ScoreboardConfig::kMaxTeams];

    bool connectWifi();
};

#endif // MLB_SCOREBOARD_H
