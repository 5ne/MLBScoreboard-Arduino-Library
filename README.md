# MLBScoreboard

An Arduino library that displays live MLB scores on [Inkplate](https://inkplate.readthedocs.io/) e-paper boards — the tiny 3-color Inkplate 2 and the larger grayscale/color Inkplate 6/6PLUS/10 family.

It fetches game data from the public (unofficial, undocumented) MLB Stats API and renders it with a board-appropriate layout. Data fetching and rendering are separate layers with no dependency running from one to the other: `MLBScoreboard` (data) has no idea Inkplate exists, and a renderer only knows how to draw an `MLBGame` array it's handed. Adding support for a new Inkplate board is a new renderer, not a rewrite -- and `MLBScoreboard` is just as usable by something that isn't drawing at all (a serial log, an MQTT publisher, a unit test).

## Status

This is a v0.1 scaffold: real network calls, real JSON parsing, real Inkplate drawing calls, and a working example for the Inkplate 2 and for one larger board. It has **not** been flashed and tested on physical hardware yet — treat it as a solid starting point to build and iterate on, not a finished, field-tested library. Test on your own board before relying on it (see "Known gaps" below).

## Architecture

```
src/
  MLBGame.h                board-agnostic game state struct (score, inning, state, ...)
  MLBParsing.h/.cpp         pure logic: MLB-API JSON -> MLBGame fields. No network, no Arduino core -- unit tested (test/)
  MLBDataSource.h/.cpp      talks to statsapi.mlb.com; a thin HTTP-fetch wrapper around MLBParsing
  MLBScoreboardLogic.h/.cpp pure logic: apply a fetch result / pick the next poll interval. No network, no Arduino core -- unit tested (test/)
  MLBScoreboard.h/.cpp      data only: WiFi, adaptive polling, deep sleep -- fetches into an MLBGame[] you read via games(); no display/rendering code, no Inkplate dependency
  renderers/
    ScoreRenderer.h          abstract render(display, games[], count, favoriteIndex) interface
    CompactRenderer.h/.cpp   Inkplate 2 (212x104, 3-color) — one game at a time
    GridRenderer.h/.cpp      larger boards — a grid of game cards
test/                      host-side unit tests for MLBParsing + MLBScoreboardLogic -- see test/README.md
```

`MLBScoreboard` only gets game data into a structure your sketch can read (`games()`, `teamCount()`, `favoriteTeamIndex()`) -- it never touches a display. To actually draw something, your sketch owns the `Inkplate` display object and a renderer, calls `scoreboard.tick()` to refresh the data, then passes `scoreboard.games()` to `renderer.render(display, ...)` and calls `display.display()` itself; see the examples below. `MLBDataSource` and the renderer classes are also usable standalone if you want fully custom orchestration.

## Data source

- `GET /api/v1/schedule?sportId=1&date=YYYY-MM-DD` — resolves a team abbreviation (`"SEA"`) to today's `gamePk`, game state (preview/live/final), baseline score, and start time. Cheap; fetched every poll.
- `GET /api/v1/game/{gamePk}/linescore` — inning, outs, balls/strikes. Only fetched while a game is actually live, to avoid hammering an API with no published rate limit or SLA.

JSON is parsed with ArduinoJson using a `DeserializationOption::Filter`, so only the handful of fields the library needs are ever materialized in memory — important on an ESP32 with roughly 300KB of usable RAM.

## Power management

`ScoreboardConfig` sets three poll intervals — pregame, live, and post-final — and `MLBScoreboard::nextPollIntervalMs()` picks the right one based on the most "urgent" tracked game. For battery installs, set `useDeepSleep = true` and call `scoreboard.sleepUntilNextPoll()` after each `tick()` (and after your own render + `display.display()` call, so the last frame is actually flushed before sleeping); the Inkplate 2 draws roughly 8µA in deep sleep, so wake → fetch → render → sleep is the whole point of running this on battery at all.

If a fetch fails (WiFi hiccup, API hiccup), `tick()` keeps the last known-good score in `games()` and marks it `isStale` rather than clearing it, so your renderer can show a small stale-data marker instead of blanking the screen — e-paper holds whatever was last drawn anyway, so silently going blank would be worse than showing slightly old data.

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

## Testing

```
./test/run_tests.sh
```

Needs only a C++17 compiler — no Arduino IDE, no ESP32 toolchain, no board, no network. It unit-tests the two files that hold this library's actual logic and have zero hardware dependency by design: `MLBParsing` (MLB-API JSON → `MLBGame` fields — game state, local-time formatting, schedule lookup, linescore merging) and `MLBScoreboardLogic` (the stale-data and poll-interval rules described above). `MLBDataSource`'s and `MLBScoreboard`'s remaining code is a thin WiFi/HTTPClient/deep-sleep wrapper around those two — not independently testable off real hardware, but also not where the risk lives (the risk is "did the unofficial API's JSON shape change," which is exactly what's covered). See `test/README.md` for the full breakdown, including two real bugs this suite caught while it was being written.

## Known gaps / next steps

- Not yet tested on physical hardware — verify pin/board settings, `HTTPClient` TLS behavior against `statsapi.mlb.com`, and actual JSON field names/shapes against a live response before relying on this for a real install. (The unit tests above cover the parsing *logic*; they can't catch the live API returning a shape the fixtures don't anticipate.)
- `GridRenderer` draws in black/white only; true-color Inkplate boards (6COLOR) would benefit from a color-aware variant.
- No NTP time sync is wired in — `MLBDataSource` uses `time()`, which needs `configTime()` called somewhere (typically in the sketch's `setup()`) to be accurate; without it, "today's date" for the schedule lookup can be wrong right after boot.
- Team logo bitmaps aren't drawn by either renderer yet, just generated by the tool.
- The renderers (`CompactRenderer`/`GridRenderer`) have no automated test coverage — they're thin, direct calls into Inkplate's drawing API with no non-hardware behavior worth asserting on; see test/README.md.

## MLB Stats API note

This uses MLB's public but **unofficial and undocumented** stats API. It can change or become unavailable without notice, and there's no published rate limit — this library polls conservatively by design, but you're using it at MLB's discretion, not under a supported contract.
