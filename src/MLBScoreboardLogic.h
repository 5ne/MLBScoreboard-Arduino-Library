#ifndef MLB_SCOREBOARD_LOGIC_H
#define MLB_SCOREBOARD_LOGIC_H

#include "MLBGame.h"

// The state-management logic behind MLBScoreboard::tick() and
// nextPollIntervalMs(), pulled out as pure functions so it can be unit
// tested without WiFi/HTTPClient/deep-sleep. Takes plain values in and
// out -- no ScoreboardConfig, no MLBDataSource -- so it has no
// dependency on anything Arduino-specific either.
namespace MLBScoreboardLogic
{

// Applies the result of one team's fetch attempt to its tracked slot:
//   - fetchSucceeded: `slot` becomes `fresh` (with isStale cleared) and
//     this returns true.
//   - fetch failed but `slot` already held valid data: `slot` is left as
//     the last known-good state, just marked isStale, and this returns
//     false. (`fresh` is ignored in this case -- a failed fetch has
//     nothing usable in it.)
//   - fetch failed and `slot` never had valid data: `slot` is left
//     untouched (isValid stays false) and this returns false.
// This is the "keep showing the last known-good score with a stale
// marker instead of going blank" contract described in the README.
bool applyFetchResult(MLBGame &slot, const MLBGame &fresh, bool fetchSucceeded);

// Picks a poll interval from the most "urgent" state across
// `games[0..count)`: any live game wins, else the earliest preview, else
// the final (basically-stopped) interval. Entries with isValid == false
// are ignored, same as an untracked team would be.
unsigned long computeNextPollIntervalMs(const MLBGame *games, int count, unsigned long pollPreviewMs,
                                         unsigned long pollLiveMs, unsigned long pollFinalMs);

} // namespace MLBScoreboardLogic

#endif // MLB_SCOREBOARD_LOGIC_H
