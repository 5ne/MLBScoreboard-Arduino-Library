// Runs the REAL MLBDataSource / MLBParsing / MLBScoreboard / CompactRenderer
// code -- unmodified production sources, not a copy -- against the REAL MLB
// Stats API, on your desktop. See test/live_run/README.md for how to build,
// run, and set breakpoints on this in VSCode.
//
// Two passes, both using the exact same production classes:
//
//   1. A direct MLBDataSource::fetchGameForTeam() call, with every field of
//      the resulting MLBGame printed explicitly -- the fastest way to see
//      exactly what got parsed out of the real API response.
//
//   2. The full MLBScoreboard::tick() path, mirroring
//      examples/Inkplate2_SingleTeam/Inkplate2_SingleTeam.ino's setup()
//      almost line for line (same classes, same call sequence), so you can
//      also verify WiFi-connect/render/orchestration logic -- everything
//      the real sketch does except the final deep-sleep call, which would
//      just hang here (see live_stubs/esp_sleep.h).
//
// Debug logging is forced on, so you'll see every [MLB DEBUG]/[MLB INFO]/
// [MLB ERROR] line the real Serial Monitor would show, plus a [DISPLAY]
// trace of every draw call CompactRenderer makes (see live_stubs/Inkplate.h).

#include "MLBDataSource.h"
#include "MLBScoreboard.h"
#include "renderers/CompactRenderer.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace
{

const char *envOr(const char *name, const char *fallback)
{
    const char *v = std::getenv(name);
    return (v && v[0]) ? v : fallback;
}

const char *stateName(GameState s)
{
    switch (s)
    {
    case GAME_STATE_PREVIEW:
        return "PREVIEW";
    case GAME_STATE_LIVE:
        return "LIVE";
    case GAME_STATE_FINAL:
        return "FINAL";
    default:
        return "UNKNOWN";
    }
}

void printGame(const MLBGame &g)
{
    printf("  gamePk           = %ld\n", g.gamePk);
    printf("  isValid           = %s\n", g.isValid ? "true" : "false");
    printf("  isStale           = %s\n", g.isStale ? "true" : "false");
    printf("  awayTeam / home  = \"%s\" @ \"%s\"\n", g.awayTeam, g.homeTeam);
    printf("  awayTeamName      = \"%s\"\n", g.awayTeamName);
    printf("  homeTeamName      = \"%s\"\n", g.homeTeamName);
    printf("  awayScore : home  = %d : %d\n", g.awayScore, g.homeScore);
    printf("  state             = %s (%d)\n", stateName(g.state), (int)g.state);
    printf("  startTimeLocal    = \"%s\"\n", g.startTimeLocal);
    printf("  inning            = %d (%s half)\n", g.inning, g.inningTopHalf ? "top" : "bottom");
    printf("  outs/balls/strikes= %d/%d/%d\n", g.outs, g.balls, g.strikes);
    printf("  lastUpdatedMs     = %lu\n", g.lastUpdatedMs);
}

} // namespace

int main()
{
    // Same knobs as the real sketch's "Configure me" block, read from the
    // environment so you can point this at your real team/timezone without
    // recompiling. Set these in .vscode/launch.json's "environment" list,
    // or from a terminal:
    //   MLB_TEAM=SF MLB_TZ_OFFSET_MIN=-420 ./test/build/live_run
    const char *team = envOr("MLB_TEAM", "SEA");
    int tzOffsetMin = atoi(envOr("MLB_TZ_OFFSET_MIN", "-420"));

    printf("==================================================================\n");
    printf(" MLBScoreboard live run -- team=%s  tzOffsetMin=%d\n", team, tzOffsetMin);
    printf(" This makes a REAL network call to statsapi.mlb.com.\n");
    printf("==================================================================\n\n");

    // ---- Pass 1: direct MLBDataSource call, full struct dump ----------
    printf("---- Pass 1: MLBDataSource::fetchGameForTeam(\"%s\") directly ----\n\n", team);

    MLBDataSource dataSource;
    dataSource.setTimezoneOffsetMinutes(tzOffsetMin);

    MLBGame game;
    bool fetchOk = dataSource.fetchGameForTeam(team, game);

    printf("\nfetchGameForTeam() returned: %s\n", fetchOk ? "true" : "false");
    printf("Resulting MLBGame struct:\n");
    printGame(game);

    if (!fetchOk)
    {
        printf("\nfetchGameForTeam() returned false. Two common reasons that are NOT\n"
               "bugs: the team has no game scheduled today (UTC \"today\", not your\n"
               "local \"today\" -- see startTimeLocal/timezone notes in the README),\n"
               "or the fetch itself failed (check the [MLB ERROR]/[HTTPClient] lines\n"
               "above for the actual HTTP status or curl error).\n");
    }

    // ---- Pass 2: the full sketch-equivalent path -----------------------
    printf("\n---- Pass 2: full MLBScoreboard::tick() path (mirrors the .ino) ----\n\n");

    Inkplate display; // Inkplate 2 no-arg constructor, exactly like the sketch
    CompactRenderer renderer;
    MLBScoreboard scoreboard(display, renderer);

    ScoreboardConfig config;
    config.wifiSsid = "unused-on-desktop";
    config.wifiPassword = "unused-on-desktop";
    config.teamAbbreviations[0] = team;
    config.teamCount = 1;
    config.favoriteTeamIndex = 0;
    config.useDeepSleep = false; // this harness drives tick() directly, once

    scoreboard.begin(config);
    scoreboard.setTimezoneOffsetMinutes(tzOffsetMin);
    scoreboard.setDebugLogging(true); // force verbose [MLB DEBUG] lines on

    printf("\n");
    bool freshened = scoreboard.tick();
    printf("\ntick() returned: %s\n", freshened ? "true (fresh data)" : "false (no fresh fetch)");

    return fetchOk ? 0 : 1;
}
