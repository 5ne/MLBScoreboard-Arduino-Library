/*
  Inkplate6_MultiGame

  Shows several teams' games at once in a grid, for the larger
  grayscale/color Inkplate boards (6, 6PLUS, 10, etc). Runs on USB power
  with a normal loop(); switch useDeepSleep on in the config if you're
  running this one on battery too.

  MLBScoreboard only fetches data into a structure -- it never touches
  the display. This sketch owns the Inkplate display and the renderer,
  and is responsible for calling render() and display.display() itself
  after each tick().

  Board setting in Arduino IDE: match your specific Inkplate model.
  Required libraries: Inkplate, ArduinoJson, and this MLBScoreboard library.
*/

#include <Inkplate.h>
#include <MLBScoreboard.h>
#include <renderers/GridRenderer.h>

const char *WIFI_SSID = "YOUR_WIFI_SSID";
const char *WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

// Track up to ScoreboardConfig::kMaxTeams (8) teams at once, using the
// MLB Stats API's own team abbreviations, from the list below.
// All 30 MLB Stats API team abbreviations (note a few diverge from the
// ESPN/Baseball-Reference codes you might expect, e.g. SF not SFG, KC
// not KCR, SD not SDP, TB not TBR, CWS not CHW):
//   AZ  Arizona Diamondbacks   MIA Miami Marlins
//   ATL Atlanta Braves         MIL Milwaukee Brewers
//   ATH Athletics              MIN Minnesota Twins
//   BAL Baltimore Orioles      NYM New York Mets
//   BOS Boston Red Sox         NYY New York Yankees
//   CHC Chicago Cubs           PHI Philadelphia Phillies
//   CWS Chicago White Sox      PIT Pittsburgh Pirates
//   CIN Cincinnati Reds        SD  San Diego Padres
//   CLE Cleveland Guardians    SF  San Francisco Giants
//   COL Colorado Rockies       SEA Seattle Mariners
//   DET Detroit Tigers         STL St. Louis Cardinals
//   HOU Houston Astros         TB  Tampa Bay Rays
//   KC  Kansas City Royals     TEX Texas Rangers
//   LAA Los Angeles Angels     TOR Toronto Blue Jays
//   LAD Los Angeles Dodgers    WSH Washington Nationals
const char *WATCHED_TEAMS[] = {"SEA", "NYY", "LAD", "ATL"};
const char *FAVORITE_TEAM = "SEA";

Inkplate display(INKPLATE_1BIT); // grayscale boards: 1-bit or 3-bit mode
GridRenderer renderer;           // columns auto-picked from game count
MLBScoreboard scoreboard;

void fetchAndRender()
{
    scoreboard.tick(); // fetch -- does not touch the display
    renderer.render(display, scoreboard.games(), scoreboard.teamCount(), scoreboard.favoriteTeamIndex());
    display.display();
}

void setup()
{
    Serial.begin(115200);
    display.begin();

    ScoreboardConfig config;
    config.wifiSsid = WIFI_SSID;
    config.wifiPassword = WIFI_PASSWORD;

    config.teamCount = sizeof(WATCHED_TEAMS) / sizeof(WATCHED_TEAMS[0]);
    for (int i = 0; i < config.teamCount; i++)
    {
        config.teamAbbreviations[i] = WATCHED_TEAMS[i];
        if (strcmp(WATCHED_TEAMS[i], FAVORITE_TEAM) == 0)
            config.favoriteTeamIndex = i;
    }

    config.pollLiveMs = 45UL * 1000UL; // a bit snappier since we're on USB power
    config.useDeepSleep = false;

    scoreboard.begin(config);
}

void loop()
{
    static unsigned long lastTick = 0;
    unsigned long interval = scoreboard.nextPollIntervalMs();

    if (lastTick == 0 || millis() - lastTick >= interval)
    {
        fetchAndRender();
        lastTick = millis();
    }
    delay(1000);
}
