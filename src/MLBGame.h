#ifndef MLB_GAME_H
#define MLB_GAME_H

// Only pulled in when actually building for Arduino -- this struct is
// plain data (char arrays, ints, a bool or two), so it doesn't need
// anything Arduino.h provides. Keeping the dependency out lets MLBGame.h
// (and anything that only needs it, like the parsing/logic unit tests
// under test/) compile as ordinary host C++ with no Arduino core
// installed. ARDUINO is defined by the Arduino build system itself.
#ifdef ARDUINO
#include <Arduino.h>
#endif

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

    // Preview detail. 9 bytes is the tight fit for formatLocalTime()'s
    // longest output, e.g. "11:10 PM" or "12:00 AM" (8 chars + NUL) --
    // a two-digit hour plus a two-digit minute plus " AM"/" PM". Caught
    // by test/test_parsing.cpp, which was failing against a 7-char
    // buffer that silently truncated exactly those times.
    char startTimeLocal[9] = ""; // "7:10 PM", pre-formatted by caller

    bool isValid = false; // false = no game found / fetch failed
    bool isStale = false; // true = this is cached data from a failed refresh, not fresh

    unsigned long lastUpdatedMs = 0; // millis() timestamp of last successful fetch
};

#endif // MLB_GAME_H
