#include "MLBDataSource.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>

namespace
{
const char *kScheduleUrlFmt = "https://statsapi.mlb.com/api/v1/schedule?sportId=1&date=%s";
const char *kLinescoreUrlFmt = "https://statsapi.mlb.com/api/v1/game/%ld/linescore";

GameState parseAbstractState(const char *s)
{
    if (!s)
        return GAME_STATE_UNKNOWN;
    if (strcmp(s, "Preview") == 0)
        return GAME_STATE_PREVIEW;
    if (strcmp(s, "Live") == 0)
        return GAME_STATE_LIVE;
    if (strcmp(s, "Final") == 0)
        return GAME_STATE_FINAL;
    return GAME_STATE_UNKNOWN;
}

// Pulls "HH:MM AM/PM" out of an ISO-8601 UTC timestamp ("2026-08-24T23:10:00Z")
// and converts using a caller-supplied UTC offset in minutes. Good enough for
// a scoreboard label; not meant to be a full timezone library.
void formatLocalTime(const char *isoUtc, int utcOffsetMinutes, char *out, size_t outLen)
{
    if (!isoUtc || strlen(isoUtc) < 16)
    {
        out[0] = '\0';
        return;
    }
    int hour = (isoUtc[11] - '0') * 10 + (isoUtc[12] - '0');
    int minute = (isoUtc[14] - '0') * 10 + (isoUtc[15] - '0');

    int totalMin = hour * 60 + minute + utcOffsetMinutes;
    totalMin = ((totalMin % 1440) + 1440) % 1440; // wrap into [0, 1440)
    int h24 = totalMin / 60;
    int m = totalMin % 60;

    const char *ampm = (h24 < 12) ? "AM" : "PM";
    int h12 = h24 % 12;
    if (h12 == 0)
        h12 = 12;

    snprintf(out, outLen, "%d:%02d %s", h12, m, ampm);
}
} // namespace

MLBDataSource::MLBDataSource(uint32_t timeoutMs) : _timeoutMs(timeoutMs)
{
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

    JsonArray dates = doc["dates"].as<JsonArray>();
    if (dates.isNull() || dates.size() == 0)
        return 0;

    JsonArray games = dates[0]["games"].as<JsonArray>();
    for (JsonObject game : games)
    {
        const char *home = game["teams"]["home"]["team"]["abbreviation"] | "";
        const char *away = game["teams"]["away"]["team"]["abbreviation"] | "";
        if (strcmp(home, teamAbbreviation) == 0 || strcmp(away, teamAbbreviation) == 0)
        {
            long gamePk = game["gamePk"] | 0L;
            _cachedGamePk = gamePk;
            strncpy(_cachedForTeam, teamAbbreviation, sizeof(_cachedForTeam) - 1);
            _cachedForTeam[sizeof(_cachedForTeam) - 1] = '\0';
            strncpy(_cachedForDate, dateBuf, sizeof(_cachedForDate) - 1);
            _cachedForDate[sizeof(_cachedForDate) - 1] = '\0';
            return gamePk;
        }
    }
    return 0; // team has no game today (off day)
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

    out.inning = doc["currentInning"] | 0;
    out.inningTopHalf = doc["isTopInning"] | true;
    out.balls = doc["balls"] | 0;
    out.strikes = doc["strikes"] | 0;
    out.outs = doc["outs"] | 0;

    // Prefer linescore's live runs count when present; schedule's score
    // field can lag by a poll cycle during a live game.
    int homeRuns = doc["teams"]["home"]["runs"] | -1;
    int awayRuns = doc["teams"]["away"]["runs"] | -1;
    if (homeRuns >= 0)
        out.homeScore = homeRuns;
    if (awayRuns >= 0)
        out.awayScore = awayRuns;

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

    JsonArray dates = doc["dates"].as<JsonArray>();
    if (dates.isNull() || dates.size() == 0)
    {
        out.isValid = false;
        return false; // off day for everyone, or bad date
    }

    JsonArray games = dates[0]["games"].as<JsonArray>();
    bool found = false;
    for (JsonObject game : games)
    {
        const char *home = game["teams"]["home"]["team"]["abbreviation"] | "";
        const char *away = game["teams"]["away"]["team"]["abbreviation"] | "";
        if (strcmp(home, teamAbbreviation) != 0 && strcmp(away, teamAbbreviation) != 0)
            continue;

        out.gamePk = game["gamePk"] | 0L;
        strncpy(out.homeTeam, home, sizeof(out.homeTeam) - 1);
        strncpy(out.awayTeam, away, sizeof(out.awayTeam) - 1);
        strncpy(out.homeTeamName, game["teams"]["home"]["team"]["name"] | "", sizeof(out.homeTeamName) - 1);
        strncpy(out.awayTeamName, game["teams"]["away"]["team"]["name"] | "", sizeof(out.awayTeamName) - 1);
        out.homeScore = game["teams"]["home"]["score"] | 0;
        out.awayScore = game["teams"]["away"]["score"] | 0;
        out.state = parseAbstractState(game["status"]["abstractGameState"] | "");
        formatLocalTime(game["gameDate"] | "", 0, out.startTimeLocal, sizeof(out.startTimeLocal));
        out.isValid = true;
        out.lastUpdatedMs = millis();
        found = true;
        break;
    }

    if (!found)
    {
        out.isValid = false;
        return false; // this team has no game today
    }

    if (out.state == GAME_STATE_LIVE)
    {
        // Layer live inning/count detail on top of the schedule snapshot.
        fetchLinescore(out.gamePk, out);
    }

    return true;
}
