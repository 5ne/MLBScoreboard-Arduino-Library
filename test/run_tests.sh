#!/usr/bin/env bash
# Builds and runs the host-side unit tests for MLBScoreboard's testable
# logic (MLBParsing + MLBScoreboardLogic). Needs nothing but a C++17
# compiler -- no Arduino IDE, no PlatformIO, no ESP32 toolchain, no
# network access. See test/README.md for what is and isn't covered here.
set -euo pipefail

cd "$(dirname "$0")/.."

CXX="${CXX:-g++}"
BUILD_DIR="test/build"
BIN="$BUILD_DIR/run_tests"

mkdir -p "$BUILD_DIR"

# The tests link against a vendored copy of ArduinoJson (see
# test/README.md for why) shipped as a tarball so the repo doesn't carry
# 130+ loose vendored files. Unpack it once, on first run.
if [ ! -f "test/vendor/ArduinoJson/ArduinoJson.h" ]; then
    echo "First run: unpacking vendored ArduinoJson..."
    tar xzf test/vendor/ArduinoJson.tar.gz -C test/vendor
fi

echo "Building tests with $CXX..."
"$CXX" -std=c++17 -Wall -Wextra \
    -I test/vendor/ArduinoJson \
    -I src \
    src/MLBParsing.cpp \
    src/MLBScoreboardLogic.cpp \
    test/test_main.cpp \
    test/test_mlbgame.cpp \
    test/test_parsing.cpp \
    test/test_responses.cpp \
    test/test_scoreboard_logic.cpp \
    -o "$BIN"

echo
"$BIN"
