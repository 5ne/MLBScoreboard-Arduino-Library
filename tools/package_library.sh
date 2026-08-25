#!/usr/bin/env bash
# Packages this repo into an Arduino-IDE-installable .zip: "Sketch ->
# Include Library -> Add .ZIP Library..." and point it at the file this
# prints. Re-run after any change under src/ (or elsewhere) to pick it
# up -- there's no separate "compile" step; Arduino compiles the library
# itself when your sketch builds, this script just repackages the
# source tree into the shape Arduino IDE expects to install.
#
# Deliberately an explicit include-list, not "zip everything minus a
# few excludes": examples/*/arduino_secrets.h holds real, gitignored
# WiFi credentials (see README) and must never end up in a zip someone
# might hand around -- an exclude-list would silently start shipping it
# again the moment anyone adds a new gitignored file that isn't listed.
set -euo pipefail

cd "$(dirname "$0")/.."

LIB_NAME="$(grep '^name=' library.properties | cut -d= -f2)"
LIB_VERSION="$(grep '^version=' library.properties | cut -d= -f2)"

DIST_DIR="dist"
STAGE_DIR="$DIST_DIR/staging/$LIB_NAME"
ZIP_PATH="$DIST_DIR/${LIB_NAME}-${LIB_VERSION}.zip"

rm -rf "$DIST_DIR/staging" "$ZIP_PATH"
mkdir -p "$STAGE_DIR"

cp library.properties keywords.txt README.md "$STAGE_DIR/"
cp -R src "$STAGE_DIR/src"

mkdir -p "$STAGE_DIR/examples"
for example_dir in examples/*/; do
    name="$(basename "$example_dir")"
    dest="$STAGE_DIR/examples/$name"
    mkdir -p "$dest"
    # Only the sketch and the *.example template -- never a real,
    # filled-in arduino_secrets.h.
    cp "$example_dir"/*.ino "$dest/"
    if [ -f "$example_dir/arduino_secrets.h.example" ]; then
        cp "$example_dir/arduino_secrets.h.example" "$dest/"
    fi
done

# Belt-and-suspenders: fail loudly instead of shipping a leaked secret
# if this script's include-list above is ever wrong.
if find "$STAGE_DIR" -name 'arduino_secrets.h' | grep -q .; then
    echo "ERROR: a real arduino_secrets.h ended up in the staged library. Aborting." >&2
    exit 1
fi

(cd "$DIST_DIR/staging" && zip -rq "../../$ZIP_PATH" "$LIB_NAME")
rm -rf "$DIST_DIR/staging"

echo "Built $ZIP_PATH"
echo "Arduino IDE: Sketch -> Include Library -> Add .ZIP Library... -> select this file"
