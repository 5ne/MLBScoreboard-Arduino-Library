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
const char *WIFI_SSID = "YOUR_WIFI_SSID";
const char *WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
const char *MY_TEAM = "SF"; // MLB Stats API team abbreviation -- e.g. SEA, NYY, LAD, SF (see README "Team abbreviations")

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
