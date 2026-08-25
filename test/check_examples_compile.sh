#!/usr/bin/env bash
# Compiles and links each examples/*/*.ino sketch AGAINST THE REAL
# src/*.cpp library sources -- not a mock, not a copy of the API surface
# -- using a small set of stub Arduino/ESP32/Inkplate headers
# (test/arduino_stubs/) so this needs nothing but a C++17 compiler, same
# as run_tests.sh. It never runs the resulting binaries; a successful
# *link* is the whole check (see test/arduino_stubs/Arduino.h for why).
#
# This exists because of a real regression: an example sketch called
# MLBScoreboard::setTimezoneOffsetMinutes() while the class declared no
# such method -- a linker error in the Arduino IDE ("undefined reference
# to ...setTimezoneOffsetMinutes..."). An earlier fix for this added a
# unit test with its OWN hand-copied mock of MLBScoreboard's API, which
# gave false confidence: the mock always "had" the method whether or not
# the real class did, so the exact same bug (missing from MLBDataSource
# this time) reappeared right after, undetected. This script builds the
# real thing instead, so there is nothing to keep in sync by hand.
#
# Every example build compiles BOTH renderers (CompactRenderer.cpp AND
# GridRenderer.cpp), not just the one the sketch happens to instantiate.
# That matches the real Arduino IDE: it compiles every .cpp under a
# library's src/ tree for any sketch that includes the library at all,
# regardless of which classes that sketch actually uses. An earlier
# version of this script only compiled each example's own renderer,
# which is exactly why a real bug in GridRenderer.cpp (INKPLATE_BLACK --
# not a real constant in the Inkplate library) shipped past this check:
# the Inkplate2 example never compiled GridRenderer.cpp, and the
# Inkplate6 example's build happened to link against a stub that
# (wrongly) defined the same macro name GridRenderer.cpp used. See
# test/README.md for the full incident.
set -euo pipefail

cd "$(dirname "$0")/.."

CXX="${CXX:-g++}"
STUBS="test/arduino_stubs"
BUILD_DIR="test/build/examples"
mkdir -p "$BUILD_DIR"

if [ ! -f "test/vendor/ArduinoJson/ArduinoJson.h" ]; then
    echo "First run: unpacking vendored ArduinoJson..."
    mkdir -p test/vendor
    tar xzf test/vendor/ArduinoJson.tar.gz -C test/vendor
fi

# arduino_secrets.h is gitignored (real credentials, per-developer), so a
# fresh clone won't have it -- seed it from the checked-in .example
# template so the compile check works out of the box. Placeholder values
# are fine here; this only needs to compile, not connect to WiFi.
for example_dir in examples/*/; do
    if [ -f "$example_dir/arduino_secrets.h.example" ] && [ ! -f "$example_dir/arduino_secrets.h" ]; then
        cp "$example_dir/arduino_secrets.h.example" "$example_dir/arduino_secrets.h"
    fi
done

# Both renderers, always -- see the block comment above for why.
COMMON_SRCS="src/MLBDataSource.cpp src/MLBParsing.cpp src/MLBScoreboard.cpp src/MLBScoreboardLogic.cpp src/MLBTeams.cpp src/MLBLogging.cpp src/renderers/CompactRenderer.cpp src/renderers/GridRenderer.cpp"

build_example() {
    local name="$1"
    local ino="$2"
    local bin="$BUILD_DIR/$name"

    echo "Compiling+linking $ino ..."
    # -include Arduino.h mirrors what the real Arduino build does: a
    # sketch's .ino never explicitly includes Arduino.h itself, because
    # the IDE/arduino-cli force-includes it ahead of the sketch source.
    # ArduinoJson's Arduino-Print/PROGMEM *serialization* support
    # (Writer<String>, Printable convertToJson, pgm_read_byte, ...) needs
    # a much fuller Arduino core than this stub provides, and our code
    # never calls serializeJson()/uses PROGMEM. ARDUINOJSON_ENABLE_ARDUINO_STRING
    # stays on (the default) because MLBDataSource::httpGetJson() now
    # deserializes from an Arduino String (http.getString()) rather than
    # streaming straight off the socket -- see the comment there for why.
    # Disabling the still-unused pieces here doesn't change anything
    # about how the real code behaves on real hardware, where the real
    # Arduino core provides all of it regardless.
    "$CXX" -std=c++17 -Wall -Wextra \
        -DARDUINO=10812 \
        -DARDUINOJSON_ENABLE_PROGMEM=0 \
        -DARDUINOJSON_ENABLE_ARDUINO_PRINT=0 \
        -include Arduino.h \
        -I "$STUBS" \
        -I test/vendor/ArduinoJson \
        -I src \
        -x c++ "$ino" \
        $COMMON_SRCS \
        "$STUBS/main_stub.cpp" \
        -o "$bin"
    echo "  OK: linked $bin"
}

build_example "Inkplate2_SingleTeam" \
    "examples/Inkplate2_SingleTeam/Inkplate2_SingleTeam.ino"

build_example "Inkplate6_MultiGame" \
    "examples/Inkplate6_MultiGame/Inkplate6_MultiGame.ino"

echo
echo "Both example sketches compiled and linked successfully."
