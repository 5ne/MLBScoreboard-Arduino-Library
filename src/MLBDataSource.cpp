#include "MLBDataSource.h"
#include "MLBParsing.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

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
        return false;

    HTTPClient http;
    http.setTimeout(_timeoutMs);
    if (!http.begin(url))
        return false;

    int code = http.GET();
    if (code != HTTP_CODE_OK)
    {
        http.end();
        return false;
    }

    DeserializationError err = filter ? deserializeJson(doc, http.getStream(), DeserializationOption::Filter(*filter))
                                       : deserializeJson(doc, http.getStream());
    http.end();
    return err == DeserializationError::Ok;
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
        struct tm tmNow;
        gmtime_r(&now, &tmNow);
        strftime(dateBuf, sizeof(dateBuf), "%Y-%m-%d", &tmNow);
    }

    // Serve from cache if we've already resolved this team/date pair.
    if (_cachedGamePk != 0 && strcmp(_cachedForTeam, teamAbbreviation) == 0 &&
        strcmp(_cachedForDate, dateBuf) == 0)
    {
        return _cachedGamePk;
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
        return 0;

    long gamePk = MLBParsing::findGamePkForTeam(doc, teamAbbreviation);
    if (gamePk != 0)
    {
        _cachedGamePk = gamePk;
        strncpy(_cachedForTeam, teamAbbreviation, sizeof(_cachedForTeam) - 1);
        _cachedForTeam[sizeof(_cachedForTeam) - 1] = '\0';
        strncpy(_cachedForDate, dateBuf, sizeof(_cachedForDate) - 1);
        _cachedForDate[sizeof(_cachedForDate) - 1] = '\0';
    }
    return gamePk;
}

bool MLBDataSource::fetchLinescore(long gamePk, MLBGame &out)
{
    if (gamePk == 0)
        return false;

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
        return false;

    MLBParsing::fillLinescoreFromJson(doc, out);

    out.gamePk = gamePk;
    out.lastUpdatedMs = millis();
    out.isValid = true;
    return true;
}

bool MLBDataSource::fetchGameForTeam(const char *teamAbbreviation, MLBGame &out)
{
    char dateBuf[11];
    time_t now;
    time(&now);
    struct tm tmNow;
    gmtime_r(&now, &tmNow);
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
        return false; // leave `out` untouched -- caller keeps last-known-good state

    if (!MLBParsing::findGameInSchedule(doc, teamAbbreviation, out, _timezoneOffsetMinutes))
        return false; // off day for everyone, bad date, or this team has no game today

    out.lastUpdatedMs = millis();

    if (out.state == GAME_STATE_LIVE)
    {
        // Layer live inning/count detail on top of the schedule snapshot.
        fetchLinescore(out.gamePk, out);
    }

    return true;
}
