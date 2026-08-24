#include "MLBScoreboard.h"
#include <WiFi.h>
#include <esp_sleep.h>

MLBScoreboard::MLBScoreboard(Inkplate &display, ScoreRenderer &renderer) : _display(display), _renderer(renderer)
{
}

void MLBScoreboard::begin(const ScoreboardConfig &config)
{
    _config = config;
    _display.begin();
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
    bool wifiOk = connectWifi();
    bool anyFreshFetch = false;

    for (int i = 0; i < _config.teamCount; i++)
    {
        MLBGame fresh;
        bool ok = wifiOk && _dataSource.fetchGameForTeam(_config.teamAbbreviations[i], fresh);

        if (ok)
        {
            fresh.isStale = false;
            _games[i] = fresh;
            anyFreshFetch = true;
        }
        else if (_games[i].isValid)
        {
            // Keep showing the last known-good state, just mark it stale.
            _games[i].isStale = true;
        }
        // else: never had valid data for this team and this fetch also
        // failed -- _games[i] stays default-constructed (isValid=false),
        // which the renderers show as "no game" / "no data".
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
    bool anyLive = false;
    bool anyPreview = false;

    for (int i = 0; i < _config.teamCount; i++)
    {
        if (!_games[i].isValid)
            continue;
        if (_games[i].state == GAME_STATE_LIVE)
            anyLive = true;
        else if (_games[i].state == GAME_STATE_PREVIEW)
            anyPreview = true;
    }

    if (anyLive)
        return _config.pollLiveMs;
    if (anyPreview)
        return _config.pollPreviewMs;
    return _config.pollFinalMs;
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
