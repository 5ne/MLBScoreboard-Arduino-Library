// Tests that load real MLB API responses from files and verify parsing works correctly.
// This catches issues where the extraction code drops or misinterprets team data.

#include "test_framework.h"
#include "../src/MLBParsing.h"
#include <ArduinoJson.h>
#include <fstream>
#include <sstream>
#include <string>
#include <cstring>

namespace {

// Helper to read file contents into a string
std::string readFile(const char* path) {
    std::ifstream file(path);
    if (!file) {
        return "";
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

JsonDocument parse(const char *json) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, json);
    if (err) {
        fprintf(stderr, "DeserializationError: %s\n", err.c_str());
    }
    return doc;
}

// Helper to check if a string matches a simple time pattern HH:MM AM/PM
bool isTimeFormatted(const char* str) {
    // Should be like "6:45 PM" or "11:10 AM"
    if (!str || strlen(str) == 0) return false;

    // Very simple check: must contain ':' and "AM" or "PM"
    const char* colon = strchr(str, ':');
    const char* ampm = strstr(str, "AM");
    if (!ampm) ampm = strstr(str, "PM");

    return colon != nullptr && ampm != nullptr;
}

} // namespace

// ---- Real API Response Tests ----

TEST(RealResponses_NoGames_ReturnsZeroForAnyTeam) {
    std::string json = readFile("test/responses/no_games.json");
    EXPECT_TRUE(json.length() > 0);

    JsonDocument doc = parse(json.c_str());

    // When there are no games, any team should return 0
    EXPECT_EQ(MLBParsing::findGamePkForTeam(doc, "NYY"), 0L);
    EXPECT_EQ(MLBParsing::findGamePkForTeam(doc, "SF"), 0L);
    EXPECT_EQ(MLBParsing::findGamePkForTeam(doc, "LAD"), 0L);
}

TEST(RealResponses_NoGames_FindGameInScheduleReturnsFalse) {
    std::string json = readFile("test/responses/no_games.json");
    EXPECT_TRUE(json.length() > 0);

    JsonDocument doc = parse(json.c_str());
    MLBGame out;

    bool found = MLBParsing::findGameInSchedule(doc, "NYY", out);
    EXPECT_FALSE(found);
    EXPECT_FALSE(out.isValid);
}

TEST(RealResponses_SomeGames_YankeesNotPlayingReturnsZero) {
    std::string json = readFile("test/responses/some_games.json");
    EXPECT_TRUE(json.length() > 0);

    JsonDocument doc = parse(json.c_str());

    // Yankees are not in the some_games.json response
    EXPECT_EQ(MLBParsing::findGamePkForTeam(doc, "NYY"), 0L);
}

TEST(RealResponses_SomeGames_YankeesNotPlayingSetsInvalid) {
    std::string json = readFile("test/responses/some_games.json");
    EXPECT_TRUE(json.length() > 0);

    JsonDocument doc = parse(json.c_str());
    MLBGame out;
    out.isValid = true;

    bool found = MLBParsing::findGameInSchedule(doc, "NYY", out);
    EXPECT_FALSE(found);
    EXPECT_FALSE(out.isValid);
}

TEST(RealResponses_SomeGames_GiantsFoundAndPopulated) {
    std::string json = readFile("test/responses/some_games.json");
    EXPECT_TRUE(json.length() > 0);

    JsonDocument doc = parse(json.c_str());

    // SF Giants should be found
    long gamePk = MLBParsing::findGamePkForTeam(doc, "SF");
    EXPECT_TRUE(gamePk != 0L);
}

TEST(RealResponses_SomeGames_GiantsGameHasValidState) {
    std::string json = readFile("test/responses/some_games.json");
    EXPECT_TRUE(json.length() > 0);

    JsonDocument doc = parse(json.c_str());
    MLBGame out;

    bool found = MLBParsing::findGameInSchedule(doc, "SF", out);
    EXPECT_TRUE(found);
    EXPECT_TRUE(out.isValid);
    EXPECT_TRUE(out.state != GAME_STATE_UNKNOWN);
    EXPECT_TRUE(out.startTimeLocal[0] != '\0');
}

TEST(RealResponses_SomeGames_GiantsGameTimeNotNoGameToday) {
    std::string json = readFile("test/responses/some_games.json");
    EXPECT_TRUE(json.length() > 0);

    JsonDocument doc = parse(json.c_str());
    MLBGame out;

    bool found = MLBParsing::findGameInSchedule(doc, "SF", out);
    EXPECT_TRUE(found);

    // The critical test: startTimeLocal should NOT be "No game today"
    EXPECT_FALSE(strcmp(out.startTimeLocal, "No game today") == 0);

    // Should either be empty or formatted with time
    if (strlen(out.startTimeLocal) > 0) {
        EXPECT_TRUE(isTimeFormatted(out.startTimeLocal));
    }
}

TEST(RealResponses_SomeGames_SFAndYankeesDistinctResults) {
    std::string json = readFile("test/responses/some_games.json");
    EXPECT_TRUE(json.length() > 0);

    JsonDocument doc = parse(json.c_str());

    // SF should find a game, Yankees should not
    long sfGamePk = MLBParsing::findGamePkForTeam(doc, "SF");
    long nyyGamePk = MLBParsing::findGamePkForTeam(doc, "NYY");

    EXPECT_TRUE(sfGamePk != 0L);
    EXPECT_EQ(nyyGamePk, 0L);
}

TEST(RealResponses_SomeGames_GiantsTimezoneConversionToPacific) {
    // Regression test for the original bug report: the SF Giants game in
    // this real captured API response starts at 2026-08-25T01:45:00Z --
    // with no timezone conversion this rendered as "1:45 AM" (and looked
    // like a stale/wrong game), instead of 6:45 PM Pacific the evening
    // before. This drives the exact function MLBDataSource::
    // fetchGameForTeam() calls, at the exact offset a US Pacific sketch
    // would configure via setTimezoneOffsetMinutes(-7 * 60), so a
    // regression here is a regression on real hardware too.
    std::string json = readFile("test/responses/some_games.json");
    EXPECT_TRUE(json.length() > 0);

    JsonDocument doc = parse(json.c_str());
    MLBGame out;

    bool found = MLBParsing::findGameInSchedule(doc, "SF", out, -7 * 60); // Pacific Daylight Time
    EXPECT_TRUE(found);
    EXPECT_TRUE(out.isValid);
    EXPECT_STREQ(out.startTimeLocal, "6:45 PM");
}

TEST(RealResponses_SomeGames_GiantsDefaultOffsetIsUtc) {
    // Same fixture, no offset argument -- confirms the UTC baseline this
    // is offset from, so the two tests together pin down both ends of
    // the conversion against a real response.
    std::string json = readFile("test/responses/some_games.json");
    EXPECT_TRUE(json.length() > 0);

    JsonDocument doc = parse(json.c_str());
    MLBGame out;

    bool found = MLBParsing::findGameInSchedule(doc, "SF", out);
    EXPECT_TRUE(found);
    EXPECT_STREQ(out.startTimeLocal, "1:45 AM");
}

TEST(RealResponses_SomeGames_MultipleTeamsCanBeLookedUp) {
    std::string json = readFile("test/responses/some_games.json");
    EXPECT_TRUE(json.length() > 0);

    JsonDocument doc = parse(json.c_str());

    long sfGamePk = MLBParsing::findGamePkForTeam(doc, "SF");
    long cinGamePk = MLBParsing::findGamePkForTeam(doc, "CIN");
    long seaGamePk = MLBParsing::findGamePkForTeam(doc, "SEA");

    EXPECT_TRUE(sfGamePk != 0L);
    EXPECT_TRUE(cinGamePk != 0L);
    EXPECT_TRUE(seaGamePk != 0L);
}
