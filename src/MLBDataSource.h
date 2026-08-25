#ifndef MLB_DATA_SOURCE_H
#define MLB_DATA_SOURCE_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include "MLBGame.h"

// Talks to the public (unofficial, undocumented) MLB Stats API and turns
// its JSON responses into MLBGame structs. Deliberately uses the
// lightweight `linescore` endpoint for polling rather than the much
// larger `feed/live` payload -- on an ESP32 with ~300KB of usable RAM,
// deserializing the full live feed is not viable, and it's unnecessary
// for a scoreboard display.
//
// This class is intentionally *thin*: it owns the HTTP fetch and JSON
// filter shape, then hands the parsed JsonDocument straight to
// MLBParsing's pure functions for the actual field extraction. That
// logic lives in exactly one place (MLBParsing.cpp) so it can be unit
// tested on a desktop compiler (see test/test_parsing.cpp and
// test/test_responses.cpp) without WiFi/HTTPClient/hardware, and so the
// tested behavior and the on-device behavior can never drift apart --
// this class has no parsing logic of its own to fall out of sync.
//
// Network note: statsapi.mlb.com has no published rate limits or SLA.
// This class is deliberately conservative about how often it's called --
// see MLBScoreboard's adaptive polling -- and every method fails soft
// (returns isValid=false) rather than throwing, since the caller is
// almost always an unattended, battery-powered display.
class MLBDataSource
{
  public:
    // maxRedirectFollow / timeoutMs are exposed mainly for testing;
    // sensible defaults are fine for normal use.
    explicit MLBDataSource(uint32_t timeoutMs = 8000);

    // Looks up today's schedule (in the given IANA-ish UTC offset, since
    // the ESP32 has no timezone database) and returns the gamePk for the
    // given team abbreviation (e.g. "SEA"), or 0 if that team has no
    // game today. utcDateOverride, if non-empty ("YYYY-MM-DD"), is used
    // instead of computing "today" -- useful for testing or for boards
    // without a reliable RTC/NTP sync.
    long findTodaysGamePk(const char *teamAbbreviation, const char *utcDateOverride = "");

    // Fetches the lightweight linescore for a known gamePk and fills in
    // `out`. Returns true on success. On any network/parse failure,
    // `out.isValid` is left false and the previous contents of `out`
    // (e.g. the last known-good score) are NOT clobbered, so the caller
    // can keep showing the last good state with a "stale" indicator.
    bool fetchLinescore(long gamePk, MLBGame &out);

    // Convenience: does findTodaysGamePk() + fetchLinescore() in one
    // call. Caches the resolved gamePk internally so repeated polls
    // during the same day don't re-hit the schedule endpoint. Uses
    // setTimezoneOffsetMinutes()'s offset (default UTC) for
    // out.startTimeLocal.
    bool fetchGameForTeam(const char *teamAbbreviation, MLBGame &out);

    // Set timezone offset in minutes from UTC for correct local time display.
    // Examples: PDT = -420 (UTC-7), MDT = -360 (UTC-6), CDT = -300 (UTC-5), EDT = -240 (UTC-4)
    void setTimezoneOffsetMinutes(int offsetMinutes);

  private:
    uint32_t _timeoutMs;
    long _cachedGamePk = 0;
    char _cachedForTeam[4] = "";
    char _cachedForDate[11] = "";
    int _timezoneOffsetMinutes = 0;

    bool httpGetJson(const String &url, JsonDocument &doc, JsonDocument *filter = nullptr);
};

#endif // MLB_DATA_SOURCE_H
