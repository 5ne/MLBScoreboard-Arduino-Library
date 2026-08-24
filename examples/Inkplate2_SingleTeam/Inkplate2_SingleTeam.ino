/*
  Inkplate2_SingleTeam

  Shows one team's game of the day on an Inkplate 2 (212x104, 3-color).
  Wakes on a timer, connects WiFi, fetches the score, draws it, then goes
  back to deep sleep -- suitable for battery power.

  MLBScoreboard only fetches data into a structure -- it never touches
  the display. This sketch owns the Inkplate display and the renderer,
  and is responsible for calling render() and display.display() itself
  after each tick().

  Board setting in Arduino IDE: "Inkplate 2"
  Required libraries: Inkplate, ArduinoJson (install via Library Manager),
  and this MLBScoreboard library.
*/

#include <Inkplate.h>
#include <MLBScoreboard.h>
#include <renderers/CompactRenderer.h>

// --- Configure me ---
const char *WIFI_SSID = "WIFI_SSID";
const char *WIFI_PASSWORD = "WIFI_PASSWORD";
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
const char *MY_TEAM = "TB"; // MLB Stats API team abbreviation, from the list above

Inkplate display; // Inkplate 2 uses the no-arg constructor (3-color mode)
CompactRenderer renderer;
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
    config.teamAbbreviations[0] = MY_TEAM;
    config.teamCount = 1;
    config.favoriteTeamIndex = 0;
    config.useDeepSleep = true; // comment out / set false while developing on USB power

    scoreboard.begin(config);
    fetchAndRender(); // fetch + render immediately on boot/wake

    if (config.useDeepSleep)
        scoreboard.sleepUntilNextPoll(); // does not return
}

void loop()
{
    // Only reached when useDeepSleep == false (e.g. bench testing on USB
    // power). Falls back to a simple delay-based loop in that case.
    static unsigned long lastTick = 0;
    unsigned long interval = scoreboard.nextPollIntervalMs();
    if (millis() - lastTick >= interval)
    {
        fetchAndRender();
        lastTick = millis();
    }
    delay(1000);
}
