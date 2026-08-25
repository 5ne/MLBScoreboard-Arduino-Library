#include "MLBScoreboard.h"
#include "MLBScoreboardLogic.h"
#include <WiFi.h>
#include <esp_sleep.h>

MLBScoreboard::MLBScoreboard(Inkplate &display, ScoreRenderer &renderer) : _display(display), _renderer(renderer)
{
}

void MLBScoreboard::begin(const ScoreboardConfig &config)
{
    _config = config;
    _display.begin();
    MLB_INFO("MLBScoreboard initialized with %d team(s)", _config.teamCount);
}

void MLBScoreboard::setTimezoneOffsetMinutes(int offsetMinutes)
{
    _dataSource.setTimezoneOffsetMinutes(offsetMinutes);
    MLB_INFO("Timezone offset set to %d minutes (UTC%+d)", offsetMinutes, offsetMinutes / 60);
}

void MLBScoreboard::setDebugLogging(bool enabled)
{
    gMLBDebugEnabled = enabled;
    MLB_INFO("Debug logging %s", enabled ? "enabled" : "disabled");
}

bool MLBScoreboard::connectWifi()
{
    if (WiFi.status() == WL_CONNECTED)
        return true;
    if (!_config.wifiSsid)
        return false;

    WiFi.mode(WIFI_STA);
    WiFi.begin(_config.wifiSsid, _config.wifiPassword);

    unsigned long start = millis();
    const unsigned long kConnectTimeoutMs = 15000;
    while (WiFi.status() != WL_CONNECTED && millis() - start < kConnectTimeoutMs)
        delay(250);

    return WiFi.status() == WL_CONNECTED;
}

bool MLBScoreboard::tick()
{
    MLB_DEBUG("Starting tick()");
    bool wifiOk = connectWifi();
    if (!wifiOk)
    {
        MLB_ERROR("WiFi connection failed");
    }
    else
    {
        MLB_DEBUG("WiFi connected");
    }

    bool anyFreshFetch = false;

    for (int i = 0; i < _config.teamCount; i++)
    {
        const char *teamAbbrev = _config.teamAbbreviations[i];
        MLB_DEBUG("Fetching game for team: %s", teamAbbrev);

        MLBGame fresh;
        bool fetchOk = wifiOk && _dataSource.fetchGameForTeam(teamAbbrev, fresh);

        // The stale/last-known-good handling is MLBScoreboardLogic::
        // applyFetchResult() (unit tested in test_scoreboard_logic.cpp),
        // not reimplemented here, so there's exactly one place that rule
        // can be wrong.
        bool freshened = MLBScoreboardLogic::applyFetchResult(_games[i], fresh, fetchOk);

        if (freshened)
        {
            anyFreshFetch = true;
            MLB_DEBUG("Team %s: %s (state=%d)", teamAbbrev, _games[i].isValid ? _games[i].startTimeLocal : "no game",
                       _games[i].state);
        }
        else if (_games[i].isValid)
        {
            // Kept showing the last known-good state, now marked stale.
            MLB_ERROR("Team %s: fetch failed, using stale data", teamAbbrev);
        }
        else
        {
            // Never had valid data for this team and this fetch also
            // failed -- _games[i] stays isValid=false, which the
            // renderers show as "no game" / "no data".
            MLB_DEBUG("Team %s: no fresh or stale data available", teamAbbrev);
        }
    }

    // Deep sleep drops WiFi anyway; disconnecting explicitly here saves a
    // little power on installs that use useDeepSleep=false but still
    // don't need WiFi between ticks.
    if (_config.useDeepSleep)
        WiFi.disconnect(true);

    _renderer.render(_display, _games, _config.teamCount, _config.favoriteTeamIndex);
    _display.display();

    return anyFreshFetch;
}

unsigned long MLBScoreboard::nextPollIntervalMs() const
{
    return MLBScoreboardLogic::computeNextPollIntervalMs(_games, _config.teamCount, _config.pollPreviewMs,
                                                           _config.pollLiveMs, _config.pollFinalMs);
}

void MLBScoreboard::sleepUntilNextPoll()
{
    unsigned long ms = nextPollIntervalMs();
    esp_sleep_enable_timer_wakeup((uint64_t)ms * 1000ULL);
    esp_deep_sleep_start();
    // esp_deep_sleep_start() never returns -- execution resumes from
    // setup() on wake, per ESP32 deep sleep semantics.
    while (true)
    {
    }
}
