#include "MLBDataSource.h"
#include "MLBParsing.h"
#include "MLBLogging.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <cstring>
#include <cstdio>
#include <ctime>

namespace
{
const char *kScheduleUrlFmt = "https://statsapi.mlb.com/api/v1/schedule?sportId=1&date=%s&hydrate=team";
const char *kLinescoreUrlFmt = "https://statsapi.mlb.com/api/v1/game/%ld/linescore";
} // namespace

MLBDataSource::MLBDataSource(uint32_t timeoutMs) : _timeoutMs(timeoutMs)
{
}

void MLBDataSource::setTimezoneOffsetMinutes(int offsetMinutes)
{
    _timezoneOffsetMinutes = offsetMinutes;
}

bool MLBDataSource::httpGetJson(const String &url, JsonDocument &doc, JsonDocument *filter)
{
    if (WiFi.status() != WL_CONNECTED)
    {
        MLB_ERROR("httpGetJson: WiFi not connected");
        return false;
    }

    MLB_DEBUG("httpGetJson: GET %s", url.c_str());
    HTTPClient http;
    http.setTimeout(_timeoutMs);
    if (!http.begin(url))
    {
        MLB_ERROR("httpGetJson: Failed to begin HTTP request");
        return false;
    }

    int code = http.GET();
    if (code != HTTP_CODE_OK)
    {
        MLB_ERROR("httpGetJson: HTTP %d", code);
        http.end();
        return false;
    }

    MLB_DEBUG("httpGetJson: Response OK, parsing JSON");
    DeserializationError err = filter ? deserializeJson(doc, http.getStream(), DeserializationOption::Filter(*filter))
                                       : deserializeJson(doc, http.getStream());
    http.end();

    if (err != DeserializationError::Ok)
    {
        MLB_ERROR("httpGetJson: JSON deserialization failed: %s", err.c_str());
        return false;
    }

    MLB_DEBUG("httpGetJson: Success");
    return true;
}

long MLBDataSource::findTodaysGamePk(const char *teamAbbreviation, const char *utcDateOverride)
{
    char dateBuf[11];
    if (utcDateOverride && utcDateOverride[0] != '\0')
    {
        strncpy(dateBuf, utcDateOverride, sizeof(dateBuf) - 1);
        dateBuf[sizeof(dateBuf) - 1] = '\0';
    }
    else
    {
        time_t now;
        time(&now);
        time_t localNow = now + static_cast<time_t>(_timezoneOffsetMinutes) * 60;
        struct tm tmNow;
        gmtime_r(&localNow, &tmNow);
        strftime(dateBuf, sizeof(dateBuf), "%Y-%m-%d", &tmNow);
    }

    MLB_DEBUG("findTodaysGamePk: Looking for %s on %s", teamAbbreviation, dateBuf);

    // Serve from cache if we've already resolved this team/date pair.
    // _cachedGamePk == -1 is a sentinel for "confirmed no game that day",
    // so an off day doesn't keep re-hitting the schedule endpoint on
    // every poll -- only 0 (never cached) misses.
    if (_cachedGamePk != 0 && strcmp(_cachedForTeam, teamAbbreviation) == 0 && strcmp(_cachedForDate, dateBuf) == 0)
    {
        long cached = (_cachedGamePk == -1) ? 0 : _cachedGamePk;
        MLB_DEBUG("findTodaysGamePk: Cache hit, gamePk=%ld", cached);
        return cached;
    }

    char urlBuf[128];
    snprintf(urlBuf, sizeof(urlBuf), kScheduleUrlFmt, dateBuf);

    // Filter the schedule payload down to just what we need -- a full
    // day's schedule.dates[].games[] response can be tens of KB, most of
    // it stats we don't want. This keeps peak RAM well under control.
    JsonDocument filter;
    filter["dates"][0]["games"][0]["gamePk"] = true;
    filter["dates"][0]["games"][0]["teams"]["home"]["team"]["abbreviation"] = true;
    filter["dates"][0]["games"][0]["teams"]["away"]["team"]["abbreviation"] = true;

    JsonDocument doc;
    if (!httpGetJson(String(urlBuf), doc, &filter))
    {
        MLB_ERROR("findTodaysGamePk: Failed to fetch schedule");
        return 0;
    }

    // Field extraction lives in MLBParsing::findGamePkForTeam (unit
    // tested against fixture JSON in test_parsing.cpp/test_responses.cpp)
    // -- not reimplemented here.
    long gamePk = MLBParsing::findGamePkForTeam(doc, teamAbbreviation);
    _cachedGamePk = (gamePk != 0) ? gamePk : -1;
    strncpy(_cachedForTeam, teamAbbreviation, sizeof(_cachedForTeam) - 1);
    _cachedForTeam[sizeof(_cachedForTeam) - 1] = '\0';
    strncpy(_cachedForDate, dateBuf, sizeof(_cachedForDate) - 1);
    _cachedForDate[sizeof(_cachedForDate) - 1] = '\0';
    if (gamePk != 0)
    {
        MLB_INFO("findTodaysGamePk: Found game for %s, gamePk=%ld", teamAbbreviation, gamePk);
    }
    else
    {
        MLB_DEBUG("findTodaysGamePk: %s has no game today, caching that", teamAbbreviation);
    }
    return gamePk;
}

bool MLBDataSource::fetchLinescore(long gamePk, MLBGame &out)
{
    if (gamePk == 0)
    {
        MLB_DEBUG("fetchLinescore: Invalid gamePk");
        return false;
    }

    MLB_DEBUG("fetchLinescore: Fetching live data for gamePk=%ld", gamePk);

    char urlBuf[96];
    snprintf(urlBuf, sizeof(urlBuf), kLinescoreUrlFmt, gamePk);

    JsonDocument filter;
    filter["currentInning"] = true;
    filter["isTopInning"] = true;
    filter["balls"] = true;
    filter["strikes"] = true;
    filter["outs"] = true;
    filter["teams"]["home"]["runs"] = true;
    filter["teams"]["away"]["runs"] = true;

    JsonDocument doc;
    if (!httpGetJson(String(urlBuf), doc, &filter))
    {
        MLB_ERROR("fetchLinescore: Failed to fetch linescore data");
        return false;
    }

    // Field extraction lives in MLBParsing::fillLinescoreFromJson (unit
    // tested in test_parsing.cpp) -- not reimplemented here.
    MLBParsing::fillLinescoreFromJson(doc, out);

    out.gamePk = gamePk;
    out.lastUpdatedMs = millis();
    out.isValid = true;

    MLB_DEBUG("fetchLinescore: Inning %d, score: %d-%d", out.inning, out.homeScore, out.awayScore);
    return true;
}

bool MLBDataSource::fetchGameForTeam(const char *teamAbbreviation, MLBGame &out)
{
    MLB_DEBUG("fetchGameForTeam: Fetching data for %s", teamAbbreviation);

    char dateBuf[11];
    time_t now;
    time(&now);
    time_t localNow = now + static_cast<time_t>(_timezoneOffsetMinutes) * 60;
    struct tm tmNow;
    gmtime_r(&localNow, &tmNow);
    strftime(dateBuf, sizeof(dateBuf), "%Y-%m-%d", &tmNow);

    // Re-fetch the schedule (cheap) every call so we pick up state
    // transitions (Preview -> Live -> Final) and the baseline score/start
    // time. The heavier linescore endpoint is only hit while the game is
    // actually live, per the class-level comment about API courtesy.
    char urlBuf[128];
    snprintf(urlBuf, sizeof(urlBuf), kScheduleUrlFmt, dateBuf);

    JsonDocument filter;
    filter["dates"][0]["games"][0]["gamePk"] = true;
    filter["dates"][0]["games"][0]["gameDate"] = true;
    filter["dates"][0]["games"][0]["status"]["abstractGameState"] = true;
    filter["dates"][0]["games"][0]["teams"]["home"]["team"]["abbreviation"] = true;
    filter["dates"][0]["games"][0]["teams"]["home"]["team"]["name"] = true;
    filter["dates"][0]["games"][0]["teams"]["home"]["score"] = true;
    filter["dates"][0]["games"][0]["teams"]["away"]["team"]["abbreviation"] = true;
    filter["dates"][0]["games"][0]["teams"]["away"]["team"]["name"] = true;
    filter["dates"][0]["games"][0]["teams"]["away"]["score"] = true;

    JsonDocument doc;
    if (!httpGetJson(String(urlBuf), doc, &filter))
    {
        MLB_ERROR("fetchGameForTeam: Failed to fetch schedule");
        return false; // leave `out` untouched -- caller keeps last-known-good state
    }

    // Field extraction (including the timezone-aware local-time format)
    // lives in MLBParsing::findGameInSchedule (unit tested against
    // fixture JSON, including real captured API responses, in
    // test_parsing.cpp/test_responses.cpp) -- not reimplemented here.
    // This is the exact function/offset the test suite exercises, so a
    // fix made there is guaranteed to be the fix that ships.
    bool found = MLBParsing::findGameInSchedule(doc, teamAbbreviation, out, _timezoneOffsetMinutes);

    if (!found)
    {
        MLB_DEBUG("fetchGameForTeam: %s has no game today", teamAbbreviation);
        return false; // this team has no game today (out.isValid already set false)
    }

    out.lastUpdatedMs = millis();
    MLB_INFO("fetchGameForTeam: %s @ %s at %s (state=%d)", out.awayTeam, out.homeTeam, out.startTimeLocal,
              out.state);

    if (out.state == GAME_STATE_LIVE)
    {
        MLB_DEBUG("fetchGameForTeam: Game is live, fetching linescore");
        // Layer live inning/count detail on top of the schedule snapshot.
        fetchLinescore(out.gamePk, out);
    }

    return true;
}
