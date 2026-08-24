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

// ---- Renderer Output Test ----

TEST(RenderOutput_GiantsPreviewGame_ShowsGameTime) {
    std::string json = readFile("test/responses/some_games.json");
    EXPECT_TRUE(json.length() > 0);

    JsonDocument doc = parse(json.c_str());
    MLBGame out;

    // Get SF Giants game (which is a Preview game at 2026-08-25T01:45:00Z)
    bool found = MLBParsing::findGameInSchedule(doc, "SF", out);
    EXPECT_TRUE(found);

    // Print what the renderer would display
    printf("\n=== SF Giants Game Details ===\n");
    printf("isValid: %d\n", out.isValid);
    printf("gamePk: %ld\n", out.gamePk);
    printf("homeTeam: %s\n", out.homeTeam);
    printf("awayTeam: %s\n", out.awayTeam);
    printf("homeTeamName: %s\n", out.homeTeamName);
    printf("awayTeamName: %s\n", out.awayTeamName);
    printf("state: %d (PREVIEW=%d, LIVE=%d, FINAL=%d, UNKNOWN=%d)\n", 
           out.state, GAME_STATE_PREVIEW, GAME_STATE_LIVE, GAME_STATE_FINAL, GAME_STATE_UNKNOWN);
    printf("startTimeLocal: '%s'\n", out.startTimeLocal);
    printf("homeScore: %d\n", out.homeScore);
    printf("awayScore: %d\n", out.awayScore);
    printf("inning: %d\n", out.inning);
    printf("inningTopHalf: %d\n", out.inningTopHalf);
    printf("isStale: %d\n", out.isStale);
    printf("=============================\n\n");

    // Verify the critical fields
    EXPECT_TRUE(out.isValid);
    EXPECT_EQ(out.state, GAME_STATE_PREVIEW);
    EXPECT_STREQ(out.homeTeam, "SF");
    EXPECT_STREQ(out.awayTeam, "CIN");
    EXPECT_FALSE(strcmp(out.startTimeLocal, "No game today") == 0);
    EXPECT_TRUE(strlen(out.startTimeLocal) > 0);
}

TEST(RenderOutput_GiantsWithPacificTimeZone) {
    std::string json = readFile("test/responses/some_games.json");
    EXPECT_TRUE(json.length() > 0);

    JsonDocument doc = parse(json.c_str());
    MLBGame out;

    // Get SF Giants game with Pacific Time offset (-7 hours = -420 minutes)
    int pacificOffsetMinutes = -7 * 60;  // PDT is UTC-7
    bool found = MLBParsing::findGameInSchedule(doc, "SF", out, pacificOffsetMinutes);
    EXPECT_TRUE(found);

    printf("\n=== SF Giants Game with Pacific Time (UTC-7) ===\n");
    printf("Game Time UTC: 2026-08-25T01:45:00Z\n");
    printf("Local Time (Pacific): %s (should be 6:45 PM on 2026-08-24)\n", out.startTimeLocal);
    printf("isValid: %d\n", out.isValid);
    printf("gamePk: %ld\n", out.gamePk);
    printf("homeTeam: %s\n", out.homeTeam);
    printf("awayTeam: %s\n", out.awayTeam);
    printf("state: %d (PREVIEW=%d)\n", out.state, GAME_STATE_PREVIEW);
    printf("===============================================\n\n");

    // Verify it's correct
    EXPECT_TRUE(out.isValid);
    EXPECT_STREQ(out.homeTeam, "SF");
    EXPECT_STREQ(out.awayTeam, "CIN");
    EXPECT_STREQ(out.startTimeLocal, "6:45 PM");
}
