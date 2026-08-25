#!/usr/bin/env bash
# Builds test/live_run/run_live.cpp against the REAL src/*.cpp sources
# (same files, unmodified) using the "live" stubs in test/live_run/live_stubs
# -- which, unlike test/arduino_stubs, actually run: real Serial output,
# real millis()/delay(), and a real HTTPS fetch via curl. See
# test/live_run/README.md.
#
# Usage:
#   ./test/live_run/build_and_run.sh                       # uses SEA / UTC-7
#   MLB_TEAM=SF MLB_TZ_OFFSET_MIN=-420 ./test/live_run/build_and_run.sh
#
# Or build only (e.g. for VSCode's debugger to launch separately):
#   ./test/live_run/build_and_run.sh --build-only
set -euo pipefail

cd "$(dirname "$0")/../.."

CXX="${CXX:-g++}"
STUBS="test/live_run/live_stubs"
BUILD_DIR="test/build"
BIN="$BUILD_DIR/live_run"
mkdir -p "$BUILD_DIR"

if [ ! -f "test/vendor/ArduinoJson/ArduinoJson.h" ]; then
    echo "First run: unpacking vendored ArduinoJson..."
    mkdir -p test/vendor
    tar xzf test/vendor/ArduinoJson.tar.gz -C test/vendor
fi

echo "Building $BIN ..."
"$CXX" -std=c++17 -g -Wall -Wextra \
    -DARDUINO=10812 \
    -DARDUINOJSON_ENABLE_PROGMEM=0 \
    -DARDUINOJSON_ENABLE_ARDUINO_PRINT=0 \
    -include Arduino.h \
    -I "$STUBS" \
    -I test/vendor/ArduinoJson \
    -I src \
    -pthread \
    test/live_run/run_live.cpp \
    src/MLBDataSource.cpp \
    src/MLBParsing.cpp \
    src/MLBScoreboard.cpp \
    src/MLBScoreboardLogic.cpp \
    src/MLBTeams.cpp \
    src/MLBLogging.cpp \
    src/renderers/CompactRenderer.cpp \
    -o "$BIN"
echo "  OK: built $BIN"

if [ "${1:-}" = "--build-only" ]; then
    exit 0
fi

echo
echo "Running $BIN ..."
echo
exec "$BIN"
