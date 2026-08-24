#include "MLBScoreboardLogic.h"

namespace MLBScoreboardLogic
{

bool applyFetchResult(MLBGame &slot, const MLBGame &fresh, bool fetchSucceeded)
{
    if (fetchSucceeded)
    {
        slot = fresh;
        slot.isStale = false;
        return true;
    }

    if (slot.isValid)
    {
        // Keep the last known-good state, just mark it stale.
        slot.isStale = true;
    }
    // else: never had valid data for this slot and this fetch also
    // failed -- leave it as-is (isValid stays false), which callers
    // should treat as "no game" / "no data".

    return false;
}

unsigned long computeNextPollIntervalMs(const MLBGame *games, int count, unsigned long pollPreviewMs,
                                         unsigned long pollLiveMs, unsigned long pollFinalMs)
{
    bool anyLive = false;
    bool anyPreview = false;

    for (int i = 0; i < count; i++)
    {
        if (!games[i].isValid)
            continue;
        if (games[i].state == GAME_STATE_LIVE)
            anyLive = true;
        else if (games[i].state == GAME_STATE_PREVIEW)
            anyPreview = true;
    }

    if (anyLive)
        return pollLiveMs;
    if (anyPreview)
        return pollPreviewMs;
    return pollFinalMs;
}

} // namespace MLBScoreboardLogic
