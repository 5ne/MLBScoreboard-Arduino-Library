#include "MLBScoreboard.h"
#include "MLBScoreboardLogic.h"
#include <WiFi.h>
#include <esp_sleep.h>
#include <ctime>

namespace
{
// The ESP32 boots with its clock at the Unix epoch and has no
// battery-backed RTC, so time(&now) reads back ~0 until NTP has synced
// -- MLBDataSource then computes "today" as 1969-12-31 or thereabouts
// (epoch, shifted by a negative timezone offset), and the schedule
// lookup finds no games for that nonsense date every time. Anything
// before this (2020-01-01 UTC) means the clock still isn't set. Deep
// sleep keeps the RTC running, so this is normally a one-time cost right
// after a cold boot/power-up, not every tick.
constexpr time_t kPlausibleEpoch = 1577836800;
} // namespace

MLBScoreboard::MLBScoreboard(Inkplate &display, ScoreRenderer &renderer) : _display(display), _renderer(renderer)
{
}

void MLBScoreboard::begin(const ScoreboardConfig &config)
{
    _config = config;
    if (_config.teamCount > ScoreboardConfig::kMaxTeams)
    {
        // teamAbbreviations[] and _games[] are both fixed at kMaxTeams --
        // an unclamped, too-large teamCount would walk off the end of
        // both in tick()'s loop. Clamp rather than assert/crash, since
        // the caller is almost always an unattended board with no one
        // watching Serial.
        MLB_ERROR("teamCount %d exceeds kMaxTeams (%d), clamping", _config.teamCount, ScoreboardConfig::kMaxTeams);
        _config.teamCount = ScoreboardConfig::kMaxTeams;
    }
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

    if (WiFi.status() != WL_CONNECTED)
        return false;

    syncTimeIfNeeded();
    return true;
}

void MLBScoreboard::syncTimeIfNeeded()
{
    time_t now;
    time(&now);
    if (now >= kPlausibleEpoch)
        return; // already synced -- e.g. survived deep sleep

    MLB_DEBUG("System clock not set, syncing via NTP");
    // gmtOffset/daylightOffset left at 0 (UTC): time(&now) always returns
    // UTC regardless of these params, and MLBDataSource applies
    // setTimezoneOffsetMinutes()'s offset itself when it needs "today".
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");

    struct tm timeinfo;
    if (!getLocalTime(&timeinfo, 10000))
    {
        MLB_ERROR("NTP time sync failed or timed out");
    }
    else
    {
        MLB_INFO("System clock synced via NTP");
    }
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
