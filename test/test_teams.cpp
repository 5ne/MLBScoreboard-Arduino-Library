// MLBTeams::lookupTeamId() is a plain string->int table, but it's worth
// covering: MLBDataSource silently falls back to an unfiltered request
// when a lookup misses (see MLBDataSource::buildScheduleUrl), so a typo
// in the table wouldn't fail loudly -- it would just quietly stop
// getting the RAM benefit of a server-side-filtered request. These
// tests are the thing that would catch that.
#include "test_framework.h"
#include "../src/MLBTeams.h"

TEST(LookupTeamId_KnownAbbreviationsResolve)
{
    // Spot-check across the ID-numbering gaps (108-121, 133-147, 158) --
    // a copy/paste slip in the table is most likely right at a gap edge.
    EXPECT_EQ(MLBTeams::lookupTeamId("LAA"), 108);
    EXPECT_EQ(MLBTeams::lookupTeamId("NYM"), 121);
    EXPECT_EQ(MLBTeams::lookupTeamId("ATH"), 133);
    EXPECT_EQ(MLBTeams::lookupTeamId("NYY"), 147);
    EXPECT_EQ(MLBTeams::lookupTeamId("MIL"), 158);
    EXPECT_EQ(MLBTeams::lookupTeamId("SEA"), 136);
}

TEST(LookupTeamId_DivergentAbbreviationsResolve)
{
    // These are exactly the codes that diverge from ESPN/Baseball-Reference
    // -- see README.md's team abbreviation note -- so they're the most
    // likely to get "corrected" back to the wrong code by mistake.
    EXPECT_EQ(MLBTeams::lookupTeamId("SF"), 137);
    EXPECT_EQ(MLBTeams::lookupTeamId("KC"), 118);
    EXPECT_EQ(MLBTeams::lookupTeamId("SD"), 135);
    EXPECT_EQ(MLBTeams::lookupTeamId("TB"), 139);
    EXPECT_EQ(MLBTeams::lookupTeamId("CWS"), 145);
    EXPECT_EQ(MLBTeams::lookupTeamId("AZ"), 109);
}

TEST(LookupTeamId_AllThirtyIdsAreUnique)
{
    static const char *kAbbreviations[] = {"ATH", "ATL", "AZ",  "BAL", "BOS", "CHC", "CIN", "CLE", "COL", "CWS",
                                            "DET", "HOU", "KC",  "LAA", "LAD", "MIA", "MIL", "MIN", "NYM", "NYY",
                                            "PHI", "PIT", "SD",  "SEA", "SF",  "STL", "TB",  "TEX", "TOR", "WSH"};
    constexpr int kCount = sizeof(kAbbreviations) / sizeof(kAbbreviations[0]);
    EXPECT_EQ(kCount, 30);

    int ids[kCount];
    for (int i = 0; i < kCount; i++)
    {
        ids[i] = MLBTeams::lookupTeamId(kAbbreviations[i]);
        EXPECT_TRUE(ids[i] != 0); // every one of the 30 must resolve
    }
    for (int i = 0; i < kCount; i++)
        for (int j = i + 1; j < kCount; j++)
            EXPECT_TRUE(ids[i] != ids[j]);
}

TEST(LookupTeamId_UnknownAbbreviationReturnsZero)
{
    EXPECT_EQ(MLBTeams::lookupTeamId("XXX"), 0);
    EXPECT_EQ(MLBTeams::lookupTeamId(""), 0);
}

TEST(LookupTeamId_NullReturnsZero)
{
    EXPECT_EQ(MLBTeams::lookupTeamId(nullptr), 0);
}

TEST(LookupTeamId_IsCaseSensitive)
{
    // The API's own abbreviations (what MLBParsing matches against) are
    // uppercase; lowercase should miss rather than silently match.
    EXPECT_EQ(MLBTeams::lookupTeamId("sea"), 0);
}
