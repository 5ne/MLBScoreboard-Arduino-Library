#ifndef MLB_PARSING_H
#define MLB_PARSING_H

#include <ArduinoJson.h>
#include "MLBGame.h"

// The MLB-Stats-API-JSON -> MLBGame logic, pulled out of MLBDataSource so
// it can be exercised without a network stack.
//
// Everything in here is a pure function: given an already-parsed
// JsonDocument (or a plain C string), it fills in an MLBGame and returns.
// No HTTP, no WiFi, no millis(), no Arduino core at all -- which is what
// makes it possible to unit-test against a hand-written JSON fixture on
// a regular desktop compiler instead of real hardware (see test/). The
// actual HTTP fetch stays in MLBDataSource, which calls these functions
// once it has a response in hand.
namespace MLBParsing
{

// Maps the schedule/linescore endpoints' "abstractGameState" string
// ("Preview" / "Live" / "Final") to a GameState. Anything else --
// including a null/missing field -- maps to GAME_STATE_UNKNOWN.
GameState parseAbstractState(const char *abstractGameState);

// Pulls "HH:MM AM/PM" out of an ISO-8601 UTC timestamp
// ("2026-08-24T23:10:00Z") and converts using a caller-supplied UTC
// offset in minutes. Good enough for a scoreboard label; not meant to be
// a full timezone library. Writes "" to `out` if `isoUtc` is null or too
// short to contain a time-of-day.
void formatLocalTime(const char *isoUtc, int utcOffsetMinutes, char *out, size_t outLen);

// Scans a `GET /schedule` response for `teamAbbreviation` (as either the
// home or away team) and returns its gamePk, or 0 if that team has no
// game today (including when the response has no games at all).
long findGamePkForTeam(JsonDocument &scheduleDoc, const char *teamAbbreviation);

// Fills `out`'s live-game fields (inning, half, count, outs) from a
// `GET /game/{gamePk}/linescore` response. Missing fields default the
// same way the API's own defaults would (0 outs/balls/strikes, top of
// the inning). Per the API's own lag between the schedule and linescore
// endpoints, home/away runs are only overwritten when the linescore
// response actually includes them -- an absent runs field leaves
// `out.homeScore`/`out.awayScore` as the caller already had them, rather
// than clobbering a good score with 0. Does NOT set out.gamePk,
// out.isValid, or out.lastUpdatedMs -- those depend on context (which
// gamePk was requested, "now") that the caller has and this function
// doesn't need.
void fillLinescoreFromJson(JsonDocument &linescoreDoc, MLBGame &out);

// Scans a `GET /schedule` response for `teamAbbreviation` and, if found,
// fills `out` with that game's teams, score, state, and start time, sets
// out.isValid = true, and returns true. If not found (including an empty
// or missing schedule), sets out.isValid = false and returns false
// without touching the rest of `out`. Does NOT set out.lastUpdatedMs --
// see fillLinescoreFromJson.
bool findGameInSchedule(JsonDocument &scheduleDoc, const char *teamAbbreviation, MLBGame &out);

} // namespace MLBParsing

#endif // MLB_PARSING_H
