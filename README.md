# MLBScoreboard

An Arduino library that displays live MLB scores on [Inkplate](https://inkplate.readthedocs.io/) e-paper boards — the tiny 3-color Inkplate 2 and the larger grayscale/color Inkplate 6/6PLUS/10 family.

It fetches game data from the public (unofficial, undocumented) MLB Stats API and renders it with a board-appropriate layout. Data fetching and rendering are separate layers, so adding support for a new Inkplate board is a new renderer, not a rewrite.

## Status

This is a v0.1 scaffold, now in active hardware bring-up on a real Inkplate board. WiFi connect, NTP-synced system clock, and the schedule-JSON fetch/parse pipeline have all been exercised against the live MLB Stats API; two real bugs surfaced this way already got fixed (system clock reading as the Unix epoch before NTP sync, and a JSON `IncompleteInput` parse failure from reading straight off the network stream — see "Timezone / system clock" and "Data source" below). On-device rendering across both example boards is still being validated. Test on your own board before relying on it for a real install (see "Known gaps" below).

The core logic (JSON parsing, stale-data/polling rules) has an automated host-side test suite that also compiles the example sketches against the real library sources — see [`test/README.md`](test/README.md) and "Tests" below.

## Architecture

```
src/
  MLBGame.h                 board-agnostic game state struct (score, inning, state, ...)
  MLBParsing.h/.cpp          pure JSON-parsing logic (no network/hardware) -- unit tested
  MLBScoreboardLogic.h/.cpp  pure stale-data/poll-interval logic (no network/hardware) -- unit tested
  MLBLogging.h/.cpp          Serial debug/info/error logging macros, shared by the two classes below
  MLBDataSource.h/.cpp       talks to statsapi.mlb.com; a thin wrapper that hands parsed JSON to MLBParsing
  MLBScoreboard.h/.cpp       orchestration: WiFi, adaptive polling, deep sleep; a thin wrapper around MLBScoreboardLogic
  renderers/
    ScoreRenderer.h          abstract render(display, games[], count, favoriteIndex) interface
    CompactRenderer.h/.cpp   Inkplate 2 (212x104, 3-color) — one game at a time
    GridRenderer.h/.cpp      larger boards — a grid of game cards
```

`MLBDataSource` and `MLBScoreboard` are deliberately thin: they own the HTTP fetch and the WiFi/deep-sleep glue respectively, but the actual decision logic (how to parse a schedule response, how to handle a failed fetch, which poll interval is next) lives once each in `MLBParsing`/`MLBScoreboardLogic`, which is what the test suite exercises. There's exactly one copy of that logic to get right, and it's the copy that ships.

Most sketches only touch `MLBScoreboard` + one renderer; `MLBDataSource`, `MLBParsing`, and the renderer classes are usable standalone if you want custom orchestration.

## Tests

```
./test/run_tests.sh
```

Needs only a C++17 compiler — no Arduino IDE, no board, no network. Runs the unit tests for `MLBParsing`/`MLBScoreboardLogic` (including against two real captured MLB API responses), then compiles and links both example sketches against the real library sources using a set of stub Arduino/Inkplate headers, so a missing or mismatched method on `MLBScoreboard`/`MLBDataSource` fails loudly here instead of in your IDE. See [`test/README.md`](test/README.md) for details.

To debug a specific "why isn't this parsing right" problem against **real, live** MLB data instead of fixtures — with actual breakpoints in VSCode, no board required — see [`test/live_run/README.md`](test/live_run/README.md).

## Timezone / system clock

The MLB Stats API returns game times in UTC; the library does not know your timezone on its own (the ESP32 has no timezone database). Configure it once, after `begin()`:

```cpp
scoreboard.setTimezoneOffsetMinutes(-7 * 60); // Pacific Daylight Time (UTC-7)
```

Common US offsets: PDT `-420`, MDT `-360`, CDT `-300`, EDT `-240` (subtract 60 more for standard time instead of daylight time). Both example sketches show this at the top of `setup()`.

Separately, the ESP32 boots with its system clock at the Unix epoch and has no battery-backed RTC, so `time()` reads back ~0 until something syncs it over the network — without that, "today" for the schedule lookup resolves to 1969-12-31. `MLBScoreboard::connectWifi()` handles this automatically: right after WiFi connects, it checks whether the clock looks plausible and, if not, syncs it via NTP (blocking up to ~10s). Nothing to configure in your sketch. Deep sleep keeps the ESP32's RTC running, so on a battery install this is normally a one-time cost right after a cold boot, not on every wake. Enable `setDebugLogging(true)` to see `System clock synced via NTP` confirm it happened.

## Debug logging

`scoreboard.setDebugLogging(true)` turns on verbose `[MLB DEBUG]` logging (API URLs, parse results, cache hits) to the Serial port, in addition to the `[MLB INFO]`/`[MLB ERROR]` lines that are always logged. Open the Arduino IDE's Serial Monitor at the sketch's baud rate (115200 in both examples) to see it.

## Data source

- `GET /api/v1/schedule?sportId=1&date=YYYY-MM-DD&teamId=...` — resolves a team abbreviation (`"SEA"`) to today's `gamePk`, game state (preview/live/final), baseline score, and start time. Cheap; fetched every poll. `teamId` is resolved internally from the abbreviation via `MLBTeams::lookupTeamId()` (a built-in table of all 30 current team IDs) and asks the API to filter server-side — one team's game instead of the whole day's ~15, which cuts the response this has to hold in RAM by roughly 9x. Falls back to an unfiltered request if the abbreviation isn't recognized.
- `GET /api/v1/game/{gamePk}/linescore` — inning, outs, balls/strikes. Only fetched while a game is actually live, to avoid hammering an API with no published rate limit or SLA.

JSON is parsed with ArduinoJson using a `DeserializationOption::Filter`, so only the handful of fields the library needs are ever materialized into the resulting `JsonDocument` — important on an ESP32 with roughly 300KB of usable RAM. The response body itself is fully received via `HTTPClient::getString()` before parsing, rather than parsed straight off the live network stream: ArduinoJson's stream reader has no retry/wait built in, and a live HTTPS stream's `available()` can transiently report 0 mid-transfer (TLS delivers data in bursts), which the reader treats as end-of-input and fails with `IncompleteInput` even though the server is still sending. `getString()` has `HTTPClient`'s own correct wait-for-more-data loop instead. The tradeoff: the raw JSON text is briefly held in RAM alongside the `JsonDocument` being built from it, not just the filtered fields.

## Power management

`ScoreboardConfig` sets three poll intervals — pregame, live, and post-final — and `MLBScoreboard::nextPollIntervalMs()` picks the right one based on the most "urgent" tracked game. For battery installs, set `useDeepSleep = true` and call `scoreboard.sleepUntilNextPoll()` after each `tick()`; the Inkplate 2 draws roughly 8µA in deep sleep, so wake → fetch → render → sleep is the whole point of running this on battery at all.

If a fetch fails (WiFi hiccup, API hiccup), the library keeps showing the last known-good score with a small stale-data marker rather than blanking the screen — e-paper holds whatever was last drawn anyway, so silently going blank would be worse than showing slightly old data.

## Examples

- `examples/Inkplate2_SingleTeam` — one favorite team, `CompactRenderer`, deep sleep between polls.
- `examples/Inkplate6_MultiGame` — several teams in a grid, `GridRenderer`, USB-powered loop.

Both need your WiFi credentials and team abbreviation(s) filled in.

WiFi credentials go in `arduino_secrets.h`, not the sketch itself, so they never end up in git history. In each example folder, copy `arduino_secrets.h.example` to `arduino_secrets.h` (same folder) and fill in your real SSID/password there:

```bash
cp examples/Inkplate2_SingleTeam/arduino_secrets.h.example examples/Inkplate2_SingleTeam/arduino_secrets.h
```

`arduino_secrets.h` is gitignored. This is the same pattern the Arduino IDE's own examples use, and it works there unmodified: extra files placed alongside a sketch's `.ino` are compiled with it, and `#include "arduino_secrets.h"` resolves to that folder.

Team abbreviation(s) are set directly at the top of the sketch (they're not secret).

## Installing local changes into Arduino IDE

If you're editing this library and want the Arduino IDE to pick up your changes:

```bash
./tools/package_library.sh
```

This builds `dist/MLBScoreboard-<version>.zip` (version read from `library.properties`). In Arduino IDE: **Sketch → Include Library → Add .ZIP Library...** and select that file. Re-run the script and re-add the zip after any further change — there's no live reload, and the IDE installs a copy of the zip's contents into `~/Documents/Arduino/libraries/`, not a link back to this repo.

The script only ever packages an explicit list of files (`library.properties`, `keywords.txt`, `README.md`, `src/`, and each example's `.ino` + `arduino_secrets.h.example`) rather than "everything minus some excludes" — real, filled-in `arduino_secrets.h` files are gitignored but still live on disk locally, and an include-list makes it structurally impossible for one to end up in a zip you hand around.

## Dependencies

Install via Arduino Library Manager:
- [Inkplate](https://github.com/SolderedElectronics/Inkplate-Arduino-library)
- [ArduinoJson](https://arduinojson.org/) (v7)

## Team logos

`tools/logo_to_bitmap.py` converts a PNG logo into a PROGMEM byte array for `drawBitmap()`, so logos are baked into the binary at compile time instead of being decoded on-device. See the script's `--help` for 1-bit (grayscale boards) and 3-color (Inkplate 2) modes. Logo rendering isn't wired into the renderers yet — the array output is ready to drop into `CompactRenderer`/`GridRenderer` when you're ready to add it.

## Known gaps / next steps

- Hardware bring-up is in progress on one physical board (Inkplate 2) — WiFi connect, NTP time sync, and the schedule fetch/parse have been confirmed working there; on-device *rendering* and the Inkplate 6-family/`GridRenderer` path are not yet confirmed on real hardware. Verify pin/board settings and actual JSON field shapes on your own board before relying on this for a real install.
- `GridRenderer` draws in black/white only; true-color Inkplate boards (6COLOR) would benefit from a color-aware variant.
- Team logo bitmaps aren't drawn by either renderer yet, just generated by the tool.
- The renderers (`CompactRenderer`/`GridRenderer`) have no automated test coverage — see [`test/README.md`](test/README.md) for why and what the highest-value version would look like once this is on real hardware.

## MLB Stats API note

This uses MLB's public but **unofficial and undocumented** stats API. It can change or become unavailable without notice, and there's no published rate limit — this library polls conservatively by design, but you're using it at MLB's discretion, not under a supported contract.
