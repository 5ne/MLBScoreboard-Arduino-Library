# Unit tests

```
./test/run_tests.sh
```

Needs nothing but a C++17 compiler (`g++` or `clang++`) on your PATH --
no Arduino IDE, no PlatformIO, no ESP32 toolchain, no board, no network.
It builds a small native binary and runs it; a non-zero exit code means
something failed.

## What's covered, and why only this

This library's actual risk isn't "does `for` still work" -- it's "did the
unofficial MLB API return JSON shaped slightly differently than expected"
and "does the stale-data/polling logic do the right thing across a poll
sequence." So the tests target exactly the two source files that hold
that logic, both of which have zero Arduino/network/hardware dependency
by design:

- **`src/MLBParsing.cpp`** (`test_parsing.cpp`) -- turns a parsed
  `JsonDocument` (or a raw ISO-8601 string) into `MLBGame` fields. Fixture
  JSON strings stand in for real API responses, covering: game-state
  string mapping, local-time formatting (including UTC-offset day
  wraparound in both directions), finding a team in a multi-game
  schedule by either home or away abbreviation, an empty/missing
  schedule, a team with no game today, and -- the one that actually
  caught a bug (see below) -- the "don't let a missing linescore runs
  field clobber a real score from the schedule endpoint" rule.
- **`src/MLBScoreboardLogic.cpp`** (`test_scoreboard_logic.cpp`) -- the
  per-tick "apply this fetch result to the tracked slot" and "which poll
  interval is next" rules that used to be inlined in
  `MLBScoreboard::tick()`/`nextPollIntervalMs()`. Covers: a successful
  fetch replacing stale data, a failed fetch preserving the last
  known-good state and flagging it stale, a team that's never had valid
  data staying invalid (not incorrectly flagged stale), and every
  live/preview/final poll-interval precedence combination.
- **`test_mlbgame.cpp`** -- sanity-checks `MLBGame`'s default field
  values, since several of the above tests depend on them.

**Not covered by an automated test, and why:**

- `MLBDataSource`'s actual HTTP calls (`httpGetJson`, and the thin
  wrappers around it in `findTodaysGamePk`/`fetchLinescore`/
  `fetchGameForTeam`) -- these need `WiFi`/`HTTPClient`, which only exist
  on the real ESP32 Arduino core. They're intentionally now *thin*: fetch
  bytes, hand them to `MLBParsing`, which is what's tested.
- `MLBScoreboard`'s WiFi connect/deep-sleep glue (`connectWifi()`,
  `sleepUntilNextPoll()`) -- same reason. It's now a thin wrapper around
  `MLBScoreboardLogic`, which is tested.
- The renderers (`CompactRenderer`, `GridRenderer`) -- these call real
  `Inkplate` drawing methods (`drawBitmap`, `setCursor`, `fillTriangle`,
  ...) with no meaningful non-hardware behavior to assert on beyond "did
  it call the right Inkplate method," which would mean either a full
  Inkplate mock or an on-device golden-image comparison. Out of scope for
  a host-side logic suite; if you want coverage here, the highest-value
  version would be an on-device visual smoke test once this library has
  actually been flashed to real hardware (see the top-level README's
  "Known gaps").

This mirrors the top-level README's own advice from before these tests
existed: "the JSON parsing logic in `MLBDataSource` is the highest-value
place to add tests." That's `MLBParsing` now.

## What this suite already caught

Writing these tests against the *existing* behavior (not the other way
around) turned up two real bugs, fixed alongside adding the tests:

1. `MLBGame::startTimeLocal` was declared `char[8]`, one byte too small
   for two-digit-hour times like `"11:10 PM"` / `"12:00 AM"` -- ArduinoJson's
   %-formatting `snprintf` silently truncated them to `"11:10 P"`. Now
   `char[9]`, the exact fit.
2. `MLBDataSource.h` forward-declared a bare `class JsonDocument` at
   global scope for a private method signature. `ArduinoJson.h` does
   `using namespace ArduinoJson;` at global scope, so any translation
   unit that included both (i.e. `MLBDataSource.cpp`) had two
   `JsonDocument` names in scope and would fail to compile with an
   "ambiguous reference" error -- on the real ESP32 toolchain, not just
   this test harness. Fixed by including `<ArduinoJson.h>` directly in
   the header and referring to the real type.

(`MLBDataSource.cpp` also picked up an explicit `#include <WiFi.h>` --
it calls `WiFi.status()`/`WL_CONNECTED` directly and was previously
relying on `<HTTPClient.h>` happening to pull that in transitively,
which isn't guaranteed across ESP32 core versions.)

## `test/vendor/ArduinoJson/`

A vendored copy of ArduinoJson v7.4.2's `src/` (MIT licensed, see
`LICENSE.txt` alongside it), used only to compile these tests on a
regular desktop machine without requiring the Arduino Library Manager's
copy to be installed on your host. It has no bearing on which
ArduinoJson version the actual library/sketches build against on
Arduino -- that's still whatever `library.properties`'
`depends=Inkplate, ArduinoJson` resolves to via Library Manager.

## Adding a test

Add a `TEST(SomeDescriptiveName) { EXPECT_EQ(...); ... }` block (see
`test_framework.h` for the tiny set of `EXPECT_*` macros) to an existing
`test_*.cpp`, or add a new `test_*.cpp` and list it in
`run_tests.sh`'s compile command.
