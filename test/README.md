# Tests

```
./test/run_tests.sh
```

Needs nothing but a C++17 compiler (`g++` or `clang++`) on your PATH --
no Arduino IDE, no PlatformIO, no ESP32 toolchain, no board, no network.
It does two things, in order, and a non-zero exit code means something
failed:

1. Builds a small native binary covering the pure-logic modules (below)
   and runs it.
2. Compiles and links both `examples/*/*.ino` sketches against the
   **real** `src/*.cpp` library sources, using a set of stub
   Arduino/ESP32/Inkplate headers (`test/arduino_stubs/`) instead of the
   real hardware libraries. See `check_examples_compile.sh` and
   "Example-sketch compile check" below for why this exists.

Debugging a specific fetch/parsing problem against **real, live** MLB
data (rather than the fixtures this suite uses) is a separate tool --
see [`test/live_run/README.md`](live_run/README.md). It runs the real
`MLBDataSource`/`MLBParsing`/`MLBScoreboard` code against the real API on
your desktop, with real VSCode breakpoints, so you don't need a board to
see exactly what got parsed.

## What's covered, and why only this

This library's actual risk isn't "does `for` still work" -- it's "did the
unofficial MLB API return JSON shaped slightly differently than expected"
and "does the stale-data/polling logic do the right thing across a poll
sequence." So the unit tests target exactly the two source files that
hold that logic, both of which have zero Arduino/network/hardware
dependency by design:

- **`src/MLBParsing.cpp`** (`test_parsing.cpp`, `test_responses.cpp`) --
  turns a parsed `JsonDocument` (or a raw ISO-8601 string) into `MLBGame`
  fields. Fixture JSON strings (plus two real captured API responses
  under `test/responses/`) stand in for live API responses, covering:
  game-state string mapping, local-time formatting *with timezone
  conversion* (including UTC-offset day wraparound in both directions --
  see "What this suite already caught" below), finding a team in a
  multi-game schedule by either home or away abbreviation, an
  empty/missing schedule, a team with no game today, and the "don't let a
  missing linescore runs field clobber a real score from the schedule
  endpoint" rule.
- **`src/MLBScoreboardLogic.cpp`** (`test_scoreboard_logic.cpp`) -- the
  per-tick "apply this fetch result to the tracked slot" and "which poll
  interval is next" rules. Covers: a successful fetch replacing stale
  data, a failed fetch preserving the last known-good state and flagging
  it stale, a team that's never had valid data staying invalid (not
  incorrectly flagged stale), and every live/preview/final poll-interval
  precedence combination.
- **`test_mlbgame.cpp`** -- sanity-checks `MLBGame`'s default field
  values, since several of the above tests depend on them.

Critically, **`MLBDataSource` and `MLBScoreboard` are thin wrappers
around these two modules, not separate reimplementations** -- see the
"Why this actually matters" section below. `MLBDataSource::
fetchGameForTeam()` calls `MLBParsing::findGameInSchedule()` directly;
`MLBScoreboard::tick()`/`nextPollIntervalMs()` call
`MLBScoreboardLogic::applyFetchResult()`/`computeNextPollIntervalMs()`
directly. There is exactly one copy of this logic, and it's the copy the
tests exercise.

**Not covered by an automated test, and why:**

- `MLBDataSource`'s actual HTTP calls (`httpGetJson`, and the thin
  wrappers around it in `findTodaysGamePk`/`fetchLinescore`/
  `fetchGameForTeam`) -- these need `WiFi`/`HTTPClient`, which only exist
  on the real ESP32 Arduino core. They're intentionally *thin*: fetch
  bytes, hand them to `MLBParsing`, which is what's tested.
- `MLBScoreboard`'s WiFi connect/deep-sleep glue (`connectWifi()`,
  `sleepUntilNextPoll()`) -- same reason. It's a thin wrapper around
  `MLBScoreboardLogic`, which is tested.
- The renderers (`CompactRenderer`, `GridRenderer`) -- these call real
  `Inkplate` drawing methods (`fillTriangle`, `setCursor`, ...) with no
  meaningful non-hardware behavior to assert on beyond "did it call the
  right Inkplate method," which would mean either a full Inkplate mock or
  an on-device golden-image comparison. Out of scope for a host-side
  logic suite; if you want coverage here, the highest-value version would
  be an on-device visual smoke test once this library has actually been
  flashed to real hardware (see the top-level README's "Known gaps").
  The example-sketch compile check below *does* compile these files, so
  a syntax/API-usage mistake in a renderer still fails loudly -- it just
  doesn't check that the pixels are right.

## Example-sketch compile check

`check_examples_compile.sh` (invoked by `run_tests.sh`, or run it
directly) compiles and links `examples/Inkplate2_SingleTeam/*.ino` and
`examples/Inkplate6_MultiGame/*.ino` against the **real**
`src/*.cpp` files, using `test/arduino_stubs/` in place of the real
Inkplate/WiFi/HTTPClient/ESP32 libraries. It never runs the resulting
binaries -- a successful *link* is the whole check, exactly like an
Arduino IDE "Verify" (see `test/arduino_stubs/Arduino.h` for the full
rationale and what's stubbed).

This exists because of a real regression, twice over:

1. An example sketch called `scoreboard.setTimezoneOffsetMinutes(...)`
   while `MLBScoreboard` declared no such method -- a compile error in
   the Arduino IDE.
2. After adding that method to `MLBScoreboard` (which delegates to
   `MLBDataSource`), `MLBDataSource` itself was missing the method --
   `undefined reference to MLBDataSource::setTimezoneOffsetMinutes(int)`
   at *link* time, past the point a plain "does it parse" check would
   catch.

An earlier fix for (1) added a unit test with its own hand-copied mock of
`MLBScoreboard`'s API surface. That gave false confidence: the mock
always "had" the method whether or not the real class did, so bug (2)
reappeared right through it, completely undetected. `check_examples_compile.sh`
builds the real thing instead -- there's nothing to keep in sync by hand,
and both classes of bug (missing declaration, missing definition) are
compile/link errors against the actual sources.

If you add a new public method to `MLBScoreboard` or `MLBDataSource` and
use it from an example sketch, this check is what catches a typo or a
forgotten `.cpp` implementation before you find out from the Arduino IDE.

Every example build compiles **both** renderers (`CompactRenderer.cpp`
*and* `GridRenderer.cpp`), not just the one that example's sketch
happens to instantiate -- because that's what the real Arduino IDE does:
it compiles every `.cpp` under a library's `src/` tree for *any* sketch
that includes the library, regardless of which classes that sketch
actually uses. Building each example against only its own renderer used
to be exactly why the regression below shipped past this check.

## A real regression: a stub that matched the bug instead of reality

`GridRenderer.cpp` called `display.setTextColor(INKPLATE_BLACK)` (and
three more call sites like it). There is no `INKPLATE_BLACK` in
SolderedElectronics/Inkplate-Arduino-library -- grayscale boards (6,
6PLUS, 10, ...) get plain `BLACK`/`WHITE` from the library's
`system/defines.h`; only the 3-color Inkplate 2 gets its own
`INKPLATE2_BLACK`/`INKPLATE2_WHITE`/`INKPLATE2_RED` (which
`CompactRenderer.cpp` uses correctly). This compiled clean in this
repo's own test suite and failed in the real Arduino IDE with:

```
error: 'INKPLATE_BLACK' was not declared in this scope; did you mean 'INKPLATE2_BLACK'?
```

It compiled clean here because `test/arduino_stubs/Inkplate.h` *also*
defined `INKPLATE_BLACK` -- not because that's a real constant, but
because whoever wrote the stub copied the name straight from
`GridRenderer.cpp`'s (already wrong) usage instead of checking it
against the real library. A stub written to match your own source
instead of the third-party API it's standing in for isn't a
verification tool; it's a mirror that nods along with whatever you show
it. Compiling only `GridRenderer.cpp` into the Inkplate6 example (and
never into the Inkplate2 example, which doesn't use it) made this worse:
there was only ever one place this bug could get caught, and the stub
was wrong in exactly that one place too.

Two fixes, both above: `test/arduino_stubs/Inkplate.h` now defines
`BLACK`/`WHITE`/`INKPLATE2_*`/`INKPLATE_1BIT`/`INKPLATE_3BIT` copied from
the real library's source (see the comment in that file for exactly
which upstream files each came from), and every example build compiles
both renderers so a mistake in either one fails every build, not just
the one example that happens to construct that renderer. If you add a
new Inkplate API constant to a renderer, verify its exact name against
the real library source first -- matching your own code back to itself
proves nothing.

## Why this actually matters (a real regression)

The "thin wrapper" architecture described above -- `MLBDataSource`/
`MLBScoreboard` delegating to `MLBParsing`/`MLBScoreboardLogic` rather
than reimplementing their logic -- was the original design, but at one
point regressed: `MLBDataSource::fetchGameForTeam()` grew its own
hand-duplicated copy of `MLBParsing::findGameInSchedule()`'s JSON
extraction, and `MLBScoreboard::tick()`/`nextPollIntervalMs()` grew their
own copies of `MLBScoreboardLogic`'s rules. The tests kept passing the
whole time, because they exercise `MLBParsing`/`MLBScoreboardLogic`
directly -- they had no way to notice that production code had quietly
stopped calling them.

The concrete symptom: a timezone-offset fix landed in the *duplicate*
copy inside `MLBDataSource.cpp` but not in `MLBParsing::
findGameInSchedule()`, so the well-tested function still hardcoded UTC
and the fix only worked by accident of which copy happened to run. This
is also why `test_responses.cpp` has both
`RealResponses_SomeGames_GiantsTimezoneConversionToPacific` and
`RealResponses_SomeGames_GiantsDefaultOffsetIsUtc` against a real
captured response -- they pin down the exact function/offset
`MLBDataSource::fetchGameForTeam()` now actually calls, so a fix made
here is guaranteed to be the fix that ships.

If you're extracting logic into a pure-function module specifically so
it can be unit tested, **delete the inline copy in the caller and call
the module** -- don't leave both. A tested module nobody calls is not a
safety net, it's a false sense of one.

## `test/vendor/ArduinoJson/`

A vendored copy of ArduinoJson v7.4.2's `src/` (MIT licensed, see
`LICENSE.txt` alongside it), used only to compile these tests on a
regular desktop machine without requiring the Arduino Library Manager's
copy to be installed on your host. It has no bearing on which
ArduinoJson version the actual library/sketches build against on
Arduino -- that's still whatever `library.properties`'
`depends=Inkplate, ArduinoJson` resolves to via Library Manager. Shipped
as a tarball (`ArduinoJson.tar.gz`) so the repo doesn't carry 130+ loose
vendored files; both `run_tests.sh` and `check_examples_compile.sh`
unpack it on first run. The unpacked tree and `test/build/` are
git-ignored -- only the tarball and the scripts are tracked.

## `test/arduino_stubs/`

Minimal stand-ins for `<Arduino.h>`, `<WiFi.h>`, `<HTTPClient.h>`,
`<esp_sleep.h>`, and `<Inkplate.h>` -- just enough of each API for the
real `src/*.cpp` files and the example `.ino` sketches to compile and
*link*, used only by `check_examples_compile.sh`. This is a build/link
check, not a hardware or network simulator: the resulting binaries are
never executed. See the comment at the top of
`test/arduino_stubs/Arduino.h` for the full rationale, including why a
few ArduinoJson features (PROGMEM, `String`/`Print` serialization
support) are disabled for this build rather than stubbed out -- our code
doesn't use them either way.

## Adding a test

Add a `TEST(SomeDescriptiveName) { EXPECT_EQ(...); ... }` block (see
`test_framework.h` for the tiny set of `EXPECT_*` macros) to an existing
`test_*.cpp`, or add a new `test_*.cpp` and list it in
`run_tests.sh`'s compile command.

If you're adding a new public method to `MLBScoreboard` or
`MLBDataSource`, also use it from whichever example sketch it's
naturally suited to (see "Example-sketch compile check" above) -- that's
what verifies the API you declared is the API you defined.
