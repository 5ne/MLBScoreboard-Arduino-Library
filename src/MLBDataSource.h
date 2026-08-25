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
    // without a reliable RTC/NTP sync. Caches per team/date, including a
    // "no game today" result, so repeated same-day polls (even on an off
    // day) don't re-hit the schedule endpoint.
    long findTodaysGamePk(const char *teamAbbreviation, const char *utcDateOverride = "");

    // Fetches the lightweight linescore for a known gamePk and fills in
    // `out`. Returns true on success. On any network/parse failure,
    // `out.isValid` is left false and the previous contents of `out`
    // (e.g. the last known-good score) are NOT clobbered, so the caller
    // can keep showing the last good state with a "stale" indicator.
    bool fetchLinescore(long gamePk, MLBGame &out);

    // Convenience: fetches today's schedule directly (not via
    // findTodaysGamePk(), and not cached -- called fresh every poll so
    // state transitions like Preview -> Live -> Final are picked up),
    // then layers on fetchLinescore() while the game is live. Uses
    // setTimezoneOffsetMinutes()'s offset (default UTC) for both
    // "today"'s date and out.startTimeLocal.
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

    // Builds the schedule request URL for `teamAbbreviation`/`dateBuf`.
    // When the abbreviation resolves via MLBTeams::lookupTeamId(), the
    // request is filtered server-side (?teamId=...) so the API returns
    // just that team's game instead of the whole day's schedule -- a
    // meaningfully smaller response to hold in RAM while parsing. Falls
    // back to the unfiltered by-date request for an unrecognized
    // abbreviation, so lookup failures degrade rather than break.
    void buildScheduleUrl(const char *teamAbbreviation, const char *dateBuf, char *urlBuf, size_t urlBufSize);
};

#endif // MLB_DATA_SOURCE_H
