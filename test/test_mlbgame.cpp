// MLBGame is a plain struct, but its default values are load-bearing:
// callers (renderers, MLBScoreboardLogic) rely on a freshly-constructed
// MLBGame reading as "no data" until something fills it in.
#include "test_framework.h"
#include "../src/MLBGame.h"

TEST(MLBGame_DefaultConstructor_IsInvalidAndNotStale)
{
    MLBGame g;
    EXPECT_FALSE(g.isValid);
    EXPECT_FALSE(g.isStale);
    EXPECT_EQ(g.state, GAME_STATE_UNKNOWN);
    EXPECT_EQ(g.gamePk, 0L);
    EXPECT_EQ(g.homeScore, 0);
    EXPECT_EQ(g.awayScore, 0);
}

TEST(MLBGame_DefaultConstructor_EmptyStrings)
{
    MLBGame g;
    EXPECT_STREQ(g.homeTeam, "");
    EXPECT_STREQ(g.awayTeam, "");
    EXPECT_STREQ(g.homeTeamName, "");
    EXPECT_STREQ(g.awayTeamName, "");
    EXPECT_STREQ(g.startTimeLocal, "");
}
