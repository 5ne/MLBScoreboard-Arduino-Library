/*
  Inkplate6_MultiGame

  Shows several teams' games at once in a grid, for the larger
  grayscale/color Inkplate boards (6, 6PLUS, 10, etc). Runs on USB power
  with a normal loop(); switch useDeepSleep on in the config if you're
  running this one on battery too.

  Board setting in Arduino IDE: match your specific Inkplate model.
  Required libraries: Inkplate, ArduinoJson, and this MLBScoreboard library.
*/

#include <Inkplate.h>
#include <MLBScoreboard.h>
#include <renderers/GridRenderer.h>

const char *WIFI_SSID = "YOUR_WIFI_SSID";
const char *WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

// Track up to ScoreboardConfig::kMaxTeams (8) teams at once.
const char *WATCHED_TEAMS[] = {"SEA", "NYY", "LAD", "ATL"};
const char *FAVORITE_TEAM = "SEA";

// Timezone offset from UTC in minutes. Set this to match your local time zone.
// Examples: PDT = -420 (UTC-7), MDT = -360 (UTC-6), CDT = -300 (UTC-5), EDT = -240 (UTC-4)
// See: https://en.wikipedia.org/wiki/Time_zone#List_of_UTC_offsets
const int TIMEZONE_OFFSET_MINUTES = -420; // Pacific Daylight Time (UTC-7)

Inkplate display(INKPLATE_1BIT); // grayscale boards: 1-bit or 3-bit mode
GridRenderer renderer;           // columns auto-picked from game count
MLBScoreboard scoreboard(display, renderer);

void setup()
{
    Serial.begin(115200);

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
    scoreboard.setTimezoneOffsetMinutes(TIMEZONE_OFFSET_MINUTES);

    // Uncomment the next line to enable debug logging in the Serial Monitor.
    // scoreboard.setDebugLogging(true);
}

void loop()
{
    static unsigned long lastTick = 0;
    unsigned long interval = scoreboard.nextPollIntervalMs();

    if (lastTick == 0 || millis() - lastTick >= interval)
    {
        scoreboard.tick();
        lastTick = millis();
    }
    delay(1000);
}
