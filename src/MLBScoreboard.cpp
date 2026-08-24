#include "MLBScoreboard.h"
#include "MLBScoreboardLogic.h"
#include <WiFi.h>
#include <esp_sleep.h>

void MLBScoreboard::begin(const ScoreboardConfig &config)
{
    _config = config;
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

        if (MLBScoreboardLogic::applyFetchResult(_games[i], fresh, ok))
            anyFreshFetch = true;
    }

    // Deep sleep drops WiFi anyway; disconnecting explicitly here saves a
    // little power on installs that use useDeepSleep=false but still
    // don't need WiFi between ticks.
    if (_config.useDeepSleep)
        WiFi.disconnect(true);

    // Rendering/display flushing is the caller's job -- see the class
    // comment in MLBScoreboard.h. This class stops at "the data is now
    // current in games()".
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
