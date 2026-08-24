// Covers MLBScoreboardLogic -- the staleness-handling and poll-interval
// rules that used to live inline in MLBScoreboard::tick() /
// nextPollIntervalMs(), where they could only be exercised by actually
// hitting WiFi + the live API. As pure functions they can be driven
// directly with hand-built MLBGame fixtures.
#include "test_framework.h"
#include "../src/MLBScoreboardLogic.h"

using namespace MLBScoreboardLogic;

namespace
{
MLBGame liveGame(long gamePk = 1)
{
    MLBGame g;
    g.gamePk = gamePk;
    g.isValid = true;
    g.state = GAME_STATE_LIVE;
    return g;
}

MLBGame previewGame(long gamePk = 1)
{
    MLBGame g;
    g.gamePk = gamePk;
    g.isValid = true;
    g.state = GAME_STATE_PREVIEW;
    return g;
}

MLBGame finalGame(long gamePk = 1)
{
    MLBGame g;
    g.gamePk = gamePk;
    g.isValid = true;
    g.state = GAME_STATE_FINAL;
    return g;
}
} // namespace

// ---- applyFetchResult ----

TEST(ApplyFetchResult_SuccessCopiesFreshDataAndClearsStale)
{
    MLBGame slot;
    slot.isValid = true;
    slot.isStale = true;
    slot.homeScore = 1;

    MLBGame fresh = liveGame(999);
    fresh.homeScore = 7;
    fresh.isStale = true; // should never survive a successful fetch

    bool freshened = applyFetchResult(slot, fresh, /*fetchSucceeded=*/true);

    EXPECT_TRUE(freshened);
    EXPECT_TRUE(slot.isValid);
    EXPECT_FALSE(slot.isStale);
    EXPECT_EQ(slot.gamePk, 999L);
    EXPECT_EQ(slot.homeScore, 7);
}

TEST(ApplyFetchResult_FailureWithPriorValidData_KeepsDataMarksStale)
{
    MLBGame slot = liveGame(42);
    slot.homeScore = 3;
    slot.awayScore = 2;

    MLBGame fresh; // whatever a failed fetch left in the scratch struct -- irrelevant
    bool freshened = applyFetchResult(slot, fresh, /*fetchSucceeded=*/false);

    EXPECT_FALSE(freshened);
    EXPECT_TRUE(slot.isValid);   // last known-good state preserved
    EXPECT_TRUE(slot.isStale);   // ...but flagged as stale
    EXPECT_EQ(slot.gamePk, 42L); // untouched
    EXPECT_EQ(slot.homeScore, 3);
    EXPECT_EQ(slot.awayScore, 2);
}

TEST(ApplyFetchResult_FailureNeverHadValidData_StaysInvalidNotStale)
{
    MLBGame slot; // never fetched successfully
    MLBGame fresh;

    bool freshened = applyFetchResult(slot, fresh, /*fetchSucceeded=*/false);

    EXPECT_FALSE(freshened);
    EXPECT_FALSE(slot.isValid);
    // Nothing to mark "stale" if there was never good data to begin with.
    EXPECT_FALSE(slot.isStale);
}

TEST(ApplyFetchResult_SuccessAfterPriorFailure_ClearsStaleFlag)
{
    MLBGame slot = liveGame(1);
    slot.isStale = true; // as left by a previous failed poll

    MLBGame fresh = liveGame(1);
    bool freshened = applyFetchResult(slot, fresh, /*fetchSucceeded=*/true);

    EXPECT_TRUE(freshened);
    EXPECT_FALSE(slot.isStale);
}

// ---- computeNextPollIntervalMs ----

const unsigned long kPreview = 15UL * 60UL * 1000UL;
const unsigned long kLive = 60UL * 1000UL;
const unsigned long kFinal = 6UL * 60UL * 60UL * 1000UL;

TEST(ComputeNextPollIntervalMs_NoGamesReturnsFinalInterval)
{
    EXPECT_EQ(computeNextPollIntervalMs(nullptr, 0, kPreview, kLive, kFinal), kFinal);
}

TEST(ComputeNextPollIntervalMs_AllInvalidEntriesReturnsFinalInterval)
{
    MLBGame games[2]; // both default-constructed, isValid == false
    EXPECT_EQ(computeNextPollIntervalMs(games, 2, kPreview, kLive, kFinal), kFinal);
}

TEST(ComputeNextPollIntervalMs_SingleLiveGameReturnsLiveInterval)
{
    MLBGame games[1] = {liveGame()};
    EXPECT_EQ(computeNextPollIntervalMs(games, 1, kPreview, kLive, kFinal), kLive);
}

TEST(ComputeNextPollIntervalMs_SinglePreviewGameReturnsPreviewInterval)
{
    MLBGame games[1] = {previewGame()};
    EXPECT_EQ(computeNextPollIntervalMs(games, 1, kPreview, kLive, kFinal), kPreview);
}

TEST(ComputeNextPollIntervalMs_AllFinalReturnsFinalInterval)
{
    MLBGame games[2] = {finalGame(1), finalGame(2)};
    EXPECT_EQ(computeNextPollIntervalMs(games, 2, kPreview, kLive, kFinal), kFinal);
}

TEST(ComputeNextPollIntervalMs_LiveGameWinsOverPreviewAndFinal)
{
    MLBGame games[3] = {previewGame(1), liveGame(2), finalGame(3)};
    EXPECT_EQ(computeNextPollIntervalMs(games, 3, kPreview, kLive, kFinal), kLive);
}

TEST(ComputeNextPollIntervalMs_PreviewWinsOverFinalWhenNoLiveGame)
{
    MLBGame games[2] = {finalGame(1), previewGame(2)};
    EXPECT_EQ(computeNextPollIntervalMs(games, 2, kPreview, kLive, kFinal), kPreview);
}

TEST(ComputeNextPollIntervalMs_IgnoresEntriesPastCount)
{
    // A live game sitting past `count` must not affect the result --
    // this is what lets a caller shrink teamCount without stale array
    // entries changing the poll cadence.
    MLBGame games[2] = {previewGame(1), liveGame(2)};
    EXPECT_EQ(computeNextPollIntervalMs(games, 1, kPreview, kLive, kFinal), kPreview);
}

TEST(ComputeNextPollIntervalMs_InvalidEntryIsIgnoredEvenIfStateLooksLive)
{
    // isValid == false means "no data for this slot" -- its `state`
    // field is meaningless leftover/default and must not be consulted.
    MLBGame games[1];
    games[0].isValid = false;
    games[0].state = GAME_STATE_LIVE;
    EXPECT_EQ(computeNextPollIntervalMs(games, 1, kPreview, kLive, kFinal), kFinal);
}
