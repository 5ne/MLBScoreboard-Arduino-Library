#include "MLBParsing.h"
#include <cstring>
#include <cstdio>
#include <cstddef>

namespace MLBParsing
{

GameState parseAbstractState(const char *abstractGameState)
{
    if (!abstractGameState)
        return GAME_STATE_UNKNOWN;
    if (strcmp(abstractGameState, "Preview") == 0)
        return GAME_STATE_PREVIEW;
    if (strcmp(abstractGameState, "Live") == 0)
        return GAME_STATE_LIVE;
    if (strcmp(abstractGameState, "Final") == 0)
        return GAME_STATE_FINAL;
    return GAME_STATE_UNKNOWN;
}

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

long findGamePkForTeam(JsonDocument &scheduleDoc, const char *teamAbbreviation)
{
    JsonArray dates = scheduleDoc["dates"].as<JsonArray>();
    if (dates.isNull() || dates.size() == 0)
        return 0; // off day for everyone, or bad date

    JsonArray games = dates[0]["games"].as<JsonArray>();
    for (JsonObject game : games)
    {
        const char *home = game["teams"]["home"]["team"]["abbreviation"] | "";
        const char *away = game["teams"]["away"]["team"]["abbreviation"] | "";
        if (strcmp(home, teamAbbreviation) == 0 || strcmp(away, teamAbbreviation) == 0)
            return game["gamePk"] | 0L;
    }
    return 0; // team has no game today (off day)
}

void fillLinescoreFromJson(JsonDocument &linescoreDoc, MLBGame &out)
{
    out.inning = linescoreDoc["currentInning"] | 0;
    out.inningTopHalf = linescoreDoc["isTopInning"] | true;
    out.balls = linescoreDoc["balls"] | 0;
    out.strikes = linescoreDoc["strikes"] | 0;
    out.outs = linescoreDoc["outs"] | 0;

    // Prefer linescore's live runs count when present; schedule's score
    // field can lag by a poll cycle during a live game.
    int homeRuns = linescoreDoc["teams"]["home"]["runs"] | -1;
    int awayRuns = linescoreDoc["teams"]["away"]["runs"] | -1;
    if (homeRuns >= 0)
        out.homeScore = homeRuns;
    if (awayRuns >= 0)
        out.awayScore = awayRuns;
}

bool findGameInSchedule(JsonDocument &scheduleDoc, const char *teamAbbreviation, MLBGame &out)
{
    JsonArray dates = scheduleDoc["dates"].as<JsonArray>();
    if (dates.isNull() || dates.size() == 0)
    {
        out.isValid = false;
        return false; // off day for everyone, or bad date
    }

    JsonArray games = dates[0]["games"].as<JsonArray>();
    for (JsonObject game : games)
    {
        const char *home = game["teams"]["home"]["team"]["abbreviation"] | "";
        const char *away = game["teams"]["away"]["team"]["abbreviation"] | "";
        if (strcmp(home, teamAbbreviation) != 0 && strcmp(away, teamAbbreviation) != 0)
            continue;

        out.gamePk = game["gamePk"] | 0L;
        strncpy(out.homeTeam, home, sizeof(out.homeTeam) - 1);
        out.homeTeam[sizeof(out.homeTeam) - 1] = '\0';
        strncpy(out.awayTeam, away, sizeof(out.awayTeam) - 1);
        out.awayTeam[sizeof(out.awayTeam) - 1] = '\0';
        strncpy(out.homeTeamName, game["teams"]["home"]["team"]["name"] | "", sizeof(out.homeTeamName) - 1);
        out.homeTeamName[sizeof(out.homeTeamName) - 1] = '\0';
        strncpy(out.awayTeamName, game["teams"]["away"]["team"]["name"] | "", sizeof(out.awayTeamName) - 1);
        out.awayTeamName[sizeof(out.awayTeamName) - 1] = '\0';
        out.homeScore = game["teams"]["home"]["score"] | 0;
        out.awayScore = game["teams"]["away"]["score"] | 0;
        out.state = parseAbstractState(game["status"]["abstractGameState"] | "");
        formatLocalTime(game["gameDate"] | "", 0, out.startTimeLocal, sizeof(out.startTimeLocal));
        out.isValid = true;
        return true;
    }

    out.isValid = false;
    return false; // this team has no game today
}

} // namespace MLBParsing
