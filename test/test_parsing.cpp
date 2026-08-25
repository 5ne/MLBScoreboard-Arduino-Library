// Covers the JSON parsing extracted into MLBParsing -- this is the
// "highest-value place to add tests" the README used to call out as a
// known gap, since it's the part of the library most exposed to the
// unofficial MLB API changing shape without notice. Every test here
// builds a JsonDocument from a hand-written fixture string, matching the
// filtered shape MLBDataSource actually requests, and checks what
// MLBParsing does with it -- no network, no ArduinoJson streaming, no
// hardware involved.
#include "test_framework.h"
#include "../src/MLBParsing.h"
#include <ArduinoJson.h>

namespace
{
JsonDocument parse(const char *json)
{
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, json);
    EXPECT_TRUE(err == DeserializationError::Ok);
    return doc;
}
} // namespace

// ---- parseAbstractState ----

TEST(ParseAbstractState_MapsKnownStates)
{
    EXPECT_EQ(MLBParsing::parseAbstractState("Preview"), GAME_STATE_PREVIEW);
    EXPECT_EQ(MLBParsing::parseAbstractState("Live"), GAME_STATE_LIVE);
    EXPECT_EQ(MLBParsing::parseAbstractState("Final"), GAME_STATE_FINAL);
}

TEST(ParseAbstractState_UnknownOrNullMapsToUnknown)
{
    EXPECT_EQ(MLBParsing::parseAbstractState("Postponed"), GAME_STATE_UNKNOWN);
    EXPECT_EQ(MLBParsing::parseAbstractState(""), GAME_STATE_UNKNOWN);
    EXPECT_EQ(MLBParsing::parseAbstractState(nullptr), GAME_STATE_UNKNOWN);
}

// ---- formatLocalTime ----

TEST(FormatLocalTime_NoOffset)
{
    char out[16];
    MLBParsing::formatLocalTime("2026-08-24T23:10:00Z", 0, out, sizeof(out));
    EXPECT_STREQ(out, "11:10 PM");
}

TEST(FormatLocalTime_NegativeOffsetSameDay)
{
    char out[16];
    // 23:10 UTC - 7h (Pacific) = 16:10 local.
    MLBParsing::formatLocalTime("2026-08-24T23:10:00Z", -7 * 60, out, sizeof(out));
    EXPECT_STREQ(out, "4:10 PM");
}

TEST(FormatLocalTime_PositiveOffsetWrapsToNextDay)
{
    char out[16];
    // 23:50 UTC + 10h wraps past midnight to 09:50 the next day.
    MLBParsing::formatLocalTime("2026-08-24T23:50:00Z", 10 * 60, out, sizeof(out));
    EXPECT_STREQ(out, "9:50 AM");
}

TEST(FormatLocalTime_NegativeOffsetWrapsToPreviousDay)
{
    char out[16];
    // 00:10 UTC - 1h wraps back to 23:10 the previous day.
    MLBParsing::formatLocalTime("2026-08-24T00:10:00Z", -60, out, sizeof(out));
    EXPECT_STREQ(out, "11:10 PM");
}

TEST(FormatLocalTime_Noon)
{
    char out[16];
    MLBParsing::formatLocalTime("2026-08-24T12:00:00Z", 0, out, sizeof(out));
    EXPECT_STREQ(out, "12:00 PM");
}

TEST(FormatLocalTime_Midnight)
{
    char out[16];
    MLBParsing::formatLocalTime("2026-08-24T00:00:00Z", 0, out, sizeof(out));
    EXPECT_STREQ(out, "12:00 AM");
}

TEST(FormatLocalTime_NullInputProducesEmptyString)
{
    char out[16] = "sentinel";
    MLBParsing::formatLocalTime(nullptr, 0, out, sizeof(out));
    EXPECT_STREQ(out, "");
}

TEST(FormatLocalTime_TooShortInputProducesEmptyString)
{
    char out[16] = "sentinel";
    MLBParsing::formatLocalTime("2026-08-24", 0, out, sizeof(out)); // date only, no time-of-day
    EXPECT_STREQ(out, "");
}

// ---- findGamePkForTeam ----

namespace
{
const char *kTwoGameSchedule = R"JSON(
{
  "dates": [
    {
      "games": [
        { "gamePk": 111, "teams": {
            "home": { "team": { "abbreviation": "BOS" } },
            "away": { "team": { "abbreviation": "TOR" } } } },
        { "gamePk": 222, "teams": {
            "home": { "team": { "abbreviation": "SEA" } },
            "away": { "team": { "abbreviation": "NYY" } } } }
      ]
    }
  ]
}
)JSON";
} // namespace

TEST(FindGamePkForTeam_MatchesHomeTeam)
{
    JsonDocument doc = parse(kTwoGameSchedule);
    EXPECT_EQ(MLBParsing::findGamePkForTeam(doc, "SEA"), 222L);
}

TEST(FindGamePkForTeam_MatchesAwayTeam)
{
    JsonDocument doc = parse(kTwoGameSchedule);
    EXPECT_EQ(MLBParsing::findGamePkForTeam(doc, "NYY"), 222L);
}

TEST(FindGamePkForTeam_ScansPastFirstGame)
{
    JsonDocument doc = parse(kTwoGameSchedule);
    EXPECT_EQ(MLBParsing::findGamePkForTeam(doc, "BOS"), 111L);
}

TEST(FindGamePkForTeam_NoMatchReturnsZero)
{
    JsonDocument doc = parse(kTwoGameSchedule);
    EXPECT_EQ(MLBParsing::findGamePkForTeam(doc, "LAD"), 0L);
}

TEST(FindGamePkForTeam_EmptyDatesReturnsZero)
{
    JsonDocument doc = parse(R"({"dates": []})");
    EXPECT_EQ(MLBParsing::findGamePkForTeam(doc, "SEA"), 0L);
}

TEST(FindGamePkForTeam_MissingDatesReturnsZero)
{
    JsonDocument doc = parse(R"({})");
    EXPECT_EQ(MLBParsing::findGamePkForTeam(doc, "SEA"), 0L);
}

// ---- fillLinescoreFromJson ----

TEST(FillLinescoreFromJson_FillsAllFields)
{
    JsonDocument doc = parse(R"JSON(
        {
          "currentInning": 7, "isTopInning": false,
          "balls": 2, "strikes": 1, "outs": 2,
          "teams": { "home": { "runs": 4 }, "away": { "runs": 5 } }
        }
    )JSON");

    MLBGame out;
    MLBParsing::fillLinescoreFromJson(doc, out);

    EXPECT_EQ(out.inning, 7);
    EXPECT_FALSE(out.inningTopHalf);
    EXPECT_EQ(out.balls, 2);
    EXPECT_EQ(out.strikes, 1);
    EXPECT_EQ(out.outs, 2);
    EXPECT_EQ(out.homeScore, 4);
    EXPECT_EQ(out.awayScore, 5);
}

TEST(FillLinescoreFromJson_MissingRunsDoesNotClobberExistingScore)
{
    // Regression test for the documented "linescore can lag a poll cycle"
    // behavior: if the runs fields are absent, the caller's existing
    // score (e.g. from the schedule endpoint) must survive untouched.
    JsonDocument doc = parse(R"JSON(
        { "currentInning": 3, "isTopInning": true, "balls": 0, "strikes": 0, "outs": 1 }
    )JSON");

    MLBGame out;
    out.homeScore = 9;
    out.awayScore = 6;
    MLBParsing::fillLinescoreFromJson(doc, out);

    EXPECT_EQ(out.homeScore, 9);
    EXPECT_EQ(out.awayScore, 6);
    EXPECT_EQ(out.inning, 3);
}

TEST(FillLinescoreFromJson_MissingFieldsDefaultToZeroOrTopHalf)
{
    JsonDocument doc = parse(R"({})");
    MLBGame out;
    MLBParsing::fillLinescoreFromJson(doc, out);

    EXPECT_EQ(out.inning, 0);
    EXPECT_TRUE(out.inningTopHalf);
    EXPECT_EQ(out.balls, 0);
    EXPECT_EQ(out.strikes, 0);
    EXPECT_EQ(out.outs, 0);
}

// ---- findGameInSchedule ----

namespace
{
const char *kOneGameSchedule = R"JSON(
{
  "dates": [
    {
      "games": [
        {
          "gamePk": 12345,
          "gameDate": "2026-08-24T23:10:00Z",
          "status": { "abstractGameState": "Live" },
          "teams": {
            "home": { "team": { "abbreviation": "SEA", "name": "Seattle Mariners" }, "score": 3 },
            "away": { "team": { "abbreviation": "NYY", "name": "New York Yankees" }, "score": 5 }
          }
        }
      ]
    }
  ]
}
)JSON";
} // namespace

TEST(FindGameInSchedule_FindsAndFillsHomeTeamMatch)
{
    JsonDocument doc = parse(kOneGameSchedule);
    MLBGame out;
    bool found = MLBParsing::findGameInSchedule(doc, "SEA", out);

    EXPECT_TRUE(found);
    EXPECT_TRUE(out.isValid);
    EXPECT_EQ(out.gamePk, 12345L);
    EXPECT_STREQ(out.homeTeam, "SEA");
    EXPECT_STREQ(out.awayTeam, "NYY");
    EXPECT_STREQ(out.homeTeamName, "Seattle Mariners");
    EXPECT_STREQ(out.awayTeamName, "New York Yankees");
    EXPECT_EQ(out.homeScore, 3);
    EXPECT_EQ(out.awayScore, 5);
    EXPECT_EQ(out.state, GAME_STATE_LIVE);
    EXPECT_STREQ(out.startTimeLocal, "11:10 PM");
}

TEST(FindGameInSchedule_FindsAwayTeamMatch)
{
    JsonDocument doc = parse(kOneGameSchedule);
    MLBGame out;
    bool found = MLBParsing::findGameInSchedule(doc, "NYY", out);

    EXPECT_TRUE(found);
    EXPECT_TRUE(out.isValid);
    EXPECT_EQ(out.gamePk, 12345L);
}

TEST(FindGameInSchedule_TeamNotPresentSetsInvalid)
{
    JsonDocument doc = parse(kOneGameSchedule);
    MLBGame out;
    out.isValid = true; // simulate stale prior state
    bool found = MLBParsing::findGameInSchedule(doc, "LAD", out);

    EXPECT_FALSE(found);
    EXPECT_FALSE(out.isValid);
}

TEST(FindGameInSchedule_EmptyDatesSetsInvalid)
{
    JsonDocument doc = parse(R"({"dates": []})");
    MLBGame out;
    bool found = MLBParsing::findGameInSchedule(doc, "SEA", out);

    EXPECT_FALSE(found);
    EXPECT_FALSE(out.isValid);
}

TEST(FindGameInSchedule_PreviewStateParsed)
{
    JsonDocument doc = parse(R"JSON(
        { "dates": [ { "games": [
            { "gamePk": 1, "gameDate": "2026-08-25T02:10:00Z",
              "status": { "abstractGameState": "Preview" },
              "teams": {
                "home": { "team": { "abbreviation": "SEA", "name": "Seattle Mariners" }, "score": 0 },
                "away": { "team": { "abbreviation": "NYY", "name": "New York Yankees" }, "score": 0 } } }
        ] } ] }
    )JSON");
    MLBGame out;
    bool found = MLBParsing::findGameInSchedule(doc, "SEA", out);

    EXPECT_TRUE(found);
    EXPECT_EQ(out.state, GAME_STATE_PREVIEW);
}

TEST(FindGameInSchedule_DefaultOffsetIsUtc)
{
    // No utcOffsetMinutes argument -- must default to 0 (UTC) so existing
    // callers (and MLBDataSource, when no timezone has been configured)
    // keep working unchanged.
    JsonDocument doc = parse(kOneGameSchedule); // gameDate: 2026-08-24T23:10:00Z
    MLBGame out;
    bool found = MLBParsing::findGameInSchedule(doc, "SEA", out);

    EXPECT_TRUE(found);
    EXPECT_STREQ(out.startTimeLocal, "11:10 PM");
}

TEST(FindGameInSchedule_AppliesTimezoneOffset)
{
    // Regression test for the "No game today" bug: MLBDataSource must
    // pass its configured timezone offset all the way through to
    // out.startTimeLocal via this function -- not just to a duplicate
    // copy of this logic that used to live in MLBDataSource.cpp.
    JsonDocument doc = parse(kOneGameSchedule); // gameDate: 2026-08-24T23:10:00Z
    MLBGame out;
    bool found = MLBParsing::findGameInSchedule(doc, "SEA", out, -7 * 60); // Pacific Daylight Time

    EXPECT_TRUE(found);
    EXPECT_STREQ(out.startTimeLocal, "4:10 PM");
}
