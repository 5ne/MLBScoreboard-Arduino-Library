# MLBScoreboard

An Arduino library that displays live MLB scores on [Inkplate](https://inkplate.readthedocs.io/) e-paper boards — the tiny 3-color Inkplate 2 and the larger grayscale/color Inkplate 6/6PLUS/10 family.

It fetches game data from the public (unofficial, undocumented) MLB Stats API and renders it with a board-appropriate layout. Data fetching and rendering are separate layers, so adding support for a new Inkplate board is a new renderer, not a rewrite.

## Status

This is a v0.1 scaffold: real network calls, real JSON parsing, real Inkplate drawing calls, and a working example for the Inkplate 2 and for one larger board. It has **not** been flashed and tested on physical hardware yet — treat it as a solid starting point to build and iterate on, not a finished, field-tested library. Test on your own board before relying on it (see "Known gaps" below).

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

## Timezone

The MLB Stats API returns game times in UTC; the library does not know your timezone on its own (the ESP32 has no timezone database). Configure it once, after `begin()`:

```cpp
scoreboard.setTimezoneOffsetMinutes(-7 * 60); // Pacific Daylight Time (UTC-7)
```

Common US offsets: PDT `-420`, MDT `-360`, CDT `-300`, EDT `-240` (subtract 60 more for standard time instead of daylight time). Both example sketches show this at the top of `setup()`.

## Debug logging

`scoreboard.setDebugLogging(true)` turns on verbose `[MLB DEBUG]` logging (API URLs, parse results, cache hits) to the Serial port, in addition to the `[MLB INFO]`/`[MLB ERROR]` lines that are always logged. Open the Arduino IDE's Serial Monitor at the sketch's baud rate (115200 in both examples) to see it.

## Data source

- `GET /api/v1/schedule?sportId=1&date=YYYY-MM-DD` — resolves a team abbreviation (`"SEA"`) to today's `gamePk`, game state (preview/live/final), baseline score, and start time. Cheap; fetched every poll.
- `GET /api/v1/game/{gamePk}/linescore` — inning, outs, balls/strikes. Only fetched while a game is actually live, to avoid hammering an API with no published rate limit or SLA.

JSON is parsed with ArduinoJson using a `DeserializationOption::Filter`, so only the handful of fields the library needs are ever materialized in memory — important on an ESP32 with roughly 300KB of usable RAM.

## Power management

`ScoreboardConfig` sets three poll intervals — pregame, live, and post-final — and `MLBScoreboard::nextPollIntervalMs()` picks the right one based on the most "urgent" tracked game. For battery installs, set `useDeepSleep = true` and call `scoreboard.sleepUntilNextPoll()` after each `tick()`; the Inkplate 2 draws roughly 8µA in deep sleep, so wake → fetch → render → sleep is the whole point of running this on battery at all.

If a fetch fails (WiFi hiccup, API hiccup), the library keeps showing the last known-good score with a small stale-data marker rather than blanking the screen — e-paper holds whatever was last drawn anyway, so silently going blank would be worse than showing slightly old data.

## Examples

- `examples/Inkplate2_SingleTeam` — one favorite team, `CompactRenderer`, deep sleep between polls.
- `examples/Inkplate6_MultiGame` — several teams in a grid, `GridRenderer`, USB-powered loop.

Both need your WiFi credentials and team abbreviation(s) filled in at the top of the sketch.

## Dependencies

Install via Arduino Library Manager:
- [Inkplate](https://github.com/SolderedElectronics/Inkplate-Arduino-library)
- [ArduinoJson](https://arduinojson.org/) (v7)

## Team logos

`tools/logo_to_bitmap.py` converts a PNG logo into a PROGMEM byte array for `drawBitmap()`, so logos are baked into the binary at compile time instead of being decoded on-device. See the script's `--help` for 1-bit (grayscale boards) and 3-color (Inkplate 2) modes. Logo rendering isn't wired into the renderers yet — the array output is ready to drop into `CompactRenderer`/`GridRenderer` when you're ready to add it.

## Known gaps / next steps

- Not yet tested on physical hardware — verify pin/board settings, `HTTPClient` TLS behavior against `statsapi.mlb.com`, and actual JSON field names/shapes against a live response before relying on this for a real install.
- `GridRenderer` draws in black/white only; true-color Inkplate boards (6COLOR) would benefit from a color-aware variant.
- No NTP time sync is wired in — `MLBDataSource` uses `time()`, which needs `configTime()` called somewhere (typically in the sketch's `setup()`) to be accurate; without it, "today's date" for the schedule lookup can be wrong right after boot.
- Team logo bitmaps aren't drawn by either renderer yet, just generated by the tool.
- The renderers (`CompactRenderer`/`GridRenderer`) have no automated test coverage — see [`test/README.md`](test/README.md) for why and what the highest-value version would look like once this is on real hardware.

## MLB Stats API note

This uses MLB's public but **unofficial and undocumented** stats API. It can change or become unavailable without notice, and there's no published rate limit — this library polls conservatively by design, but you're using it at MLB's discretion, not under a supported contract.
