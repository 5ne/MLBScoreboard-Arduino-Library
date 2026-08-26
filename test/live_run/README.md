# Live-fetch debug harness

Runs the **real, unmodified** `MLBDataSource`/`MLBParsing`/`MLBScoreboard`/
`CompactRenderer` source files against the **real** MLB Stats API, on your
Mac, with real breakpoints in VSCode -- no board, no Arduino IDE, no
flashing required. This is for the "why isn't this parsing correctly"
question specifically: it lets you inspect exactly what came back from the
API and exactly what the library did with it, without the Serial Monitor /
flash / wait / squint-at-the-e-paper loop.

This is a different tool from `./test/run_tests.sh`, and deliberately so:

| | `test/run_tests.sh` | `test/live_run` |
|---|---|---|
| Network | none -- fixtures + captured responses | **real** HTTPS call to statsapi.mlb.com |
| Purpose | fast, deterministic regression suite | interactive debugging of *your* current data |
| Stubs | `test/arduino_stubs/` -- inert, build/link only, never run | `test/live_run/live_stubs/` -- Serial really prints, HTTPClient really fetches |
| When to use | every change, in CI, before committing | tracking down "it's not parsing right" *right now* |

Both stub sets exist for the same reason `MLBDataSource`/`MLBScoreboard` are
thin wrappers around `MLBParsing`/`MLBScoreboardLogic` (see the top-level
`test/README.md`): so there is exactly one copy of the real source being
exercised, never a hand-copied mock that can drift from what you actually
ship.

## Quick start (terminal)

```
./test/live_run/build_and_run.sh
```

Uses team `SEA` / UTC-7 by default. To match your real sketch:

```
MLB_TEAM=SF MLB_TZ_OFFSET_MIN=-420 ./test/live_run/build_and_run.sh
```

You'll see two passes:

1. A direct `MLBDataSource::fetchGameForTeam("SF")` call with every field
   of the resulting `MLBGame` struct printed -- score, state, inning,
   `startTimeLocal`, `isValid`, `isStale`, all of it. This is the fastest
   way to see exactly what got parsed.
2. The full `MLBScoreboard::tick()` path, mirroring
   `examples/Inkplate2_SingleTeam/Inkplate2_SingleTeam.ino`'s `setup()`
   almost line for line -- same classes, same call order -- with a
   `[DISPLAY]` trace of every draw call `CompactRenderer` makes, so you can
   also see the rendering logic's output as text instead of on real
   e-paper.

Debug logging is forced on, so `[MLB DEBUG]`/`[MLB INFO]`/`[MLB ERROR]`
lines print too -- including the exact URL fetched and the raw HTTP
status/curl error if the request itself failed.

## Debugging in VSCode

Open this repo's folder in VSCode (the one with `.vscode/` in it -- that's
what makes the configs below show up).

1. **Set a breakpoint.** The interesting places are usually inside
   `src/MLBParsing.cpp` (`findGameInSchedule`, `formatLocalTime`) or
   `src/MLBDataSource.cpp` (`fetchGameForTeam`, `httpGetJson`) -- click in
   the gutter next to a line number.
2. **Run and Debug** (the sidebar icon, or `Cmd+Shift+D`), then pick one of:
   - **"Debug live_run (cpptools / lldb)"** -- if you have Microsoft's
     C/C++ extension installed.
   - **"Debug live_run (CodeLLDB)"** -- if you have the CodeLLDB extension
     installed instead (`vadimcn.vscode-lldb` in the marketplace). This one
     tends to be the smoother experience on macOS if you don't already have
     a preference.

   Either config runs the build task first automatically, then launches
   under the debugger stopped at your breakpoint, with `MLB_TEAM=SF` /
   `MLB_TZ_OFFSET_MIN=-420` set as the environment (edit `.vscode/launch.json`
   to change these, or your team/offset).
3. Step through with the normal controls (F10 step over, F11 step into,
   `Cmd+Shift+F5` restart). Hover any variable -- `doc`, `out`, `g` -- to see
   its live value, exactly like debugging any other C++ program, because
   that's exactly what this is: real `g++`/`clang++`-compiled code, real
   `lldb`, no simulator involved.

If you'd rather just build without launching the debugger (e.g. to run it
plain in a terminal), use the **"Build live_run..."** task directly
(`Cmd+Shift+B`, or Terminal > Run Task).

## How the real network call works

`test/live_run/live_stubs/HTTPClient.h`'s `GET()` shells out to `curl`
(already on your Mac) and reads the full response into an in-memory
buffer before returning, which `getString()` hands back as a `String` --
exactly the shape `MLBDataSource::httpGetJson()` parses via
`deserializeJson(doc, body, ...)` on real hardware too (see that
function's own comment for why it's a fully-received `String` and not a
live stream). This is why the harness needs nothing beyond a C++17
compiler and `curl` -- no libcurl headers, no TLS library to link, no
mock server.

## Things that are genuinely different from the ESP32

Worth knowing so you don't chase a phantom:

- **Timezone / "today".** `findTodaysGamePk`/`fetchGameForTeam` compute
  "today" from `time()`, shifted by the configured timezone offset. Your
  Mac's system clock is always correct; the ESP32's is not until
  `MLBScoreboard::connectWifi()` syncs it via NTP on first connect (see
  the top-level README's "Timezone / system clock" section). If the
  device shows "no game today" but this harness finds one, an unsynced
  on-device clock (or a WiFi connect that never succeeded, so
  `syncTimeIfNeeded()` never ran) is a likely suspect, not a parsing bug
  -- check the Serial log for `System clock synced via NTP`.
- **No RAM ceiling.** The `DeserializationOption::Filter` still applies
  (same filter shapes as production), but your Mac won't surface an
  ESP32-specific out-of-memory failure the way real hardware might on a
  larger-than-expected response.
- **No real TLS handshake failure modes.** `curl` handles TLS; a cert or
  handshake problem specific to the ESP32's TLS stack won't reproduce
  here. An HTTP-level problem (wrong URL, 404, unexpected JSON shape) will.

If this harness parses correctly but the device still doesn't, the bug is
almost certainly in one of the two things above, or in device-side
WiFi/deep-sleep/timing -- not in `MLBParsing`, since this harness exercises
that exact same code.
