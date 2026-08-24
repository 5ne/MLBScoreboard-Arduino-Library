#ifndef MLB_GAME_H
#define MLB_GAME_H

#include <Arduino.h>

// Board-agnostic representation of one game's state. Nothing in this
// struct knows about pixels, colors, or which Inkplate board it will
// end up on -- that's the renderers' job.

enum GameState
{
    GAME_STATE_UNKNOWN = 0,
    GAME_STATE_PREVIEW, // scheduled, not started
    GAME_STATE_LIVE,    // in progress
    GAME_STATE_FINAL    // completed
};

struct MLBGame
{
    long gamePk = 0;

    char homeTeam[4] = ""; // abbreviation, e.g. "SEA"
    char awayTeam[4] = ""; // abbreviation, e.g. "NYY"
    char homeTeamName[32] = "";
    char awayTeamName[32] = "";

    int homeScore = 0;
    int awayScore = 0;

    GameState state = GAME_STATE_UNKNOWN;

    // Live-game detail (only meaningful when state == GAME_STATE_LIVE)
    int inning = 0;
    bool inningTopHalf = true;
    int outs = 0;
    int balls = 0;
    int strikes = 0;

    // Preview detail
    char startTimeLocal[8] = ""; // "7:10 PM", pre-formatted by caller

    bool isValid = false; // false = no game found / fetch failed
    bool isStale = false; // true = this is cached data from a failed refresh, not fresh

    unsigned long lastUpdatedMs = 0; // millis() timestamp of last successful fetch
};

#endif // MLB_GAME_H
