#ifndef MLB_STUB_INKPLATE_H
#define MLB_STUB_INKPLATE_H

// Minimal stand-in for SolderedElectronics/Inkplate-Arduino-library --
// just the drawing surface used by CompactRenderer/GridRenderer and the
// example sketches. See Arduino.h in this directory for why this stub
// exists (a build/link check, never actually run).

#include <cstdint>
#include <cstring>

// Color/mode constants used across the renderers and example sketches.
//
// These are copied from the REAL library, not invented -- verified
// against SolderedElectronics/Inkplate-Arduino-library's
// src/system/defines.h (grayscale-board branch) and
// src/boards/Inkplate2/* (the Inkplate 2 is a 3-color board, so it gets
// its own INKPLATE2_* constants instead of plain BLACK/WHITE). If you
// add a new constant here, verify it against the real library source
// first -- a name that merely matches what our own code already uses is
// not verification, it's begging the question. That's exactly how
// INKPLATE_BLACK/INKPLATE_WHITE ended up defined here: they matched
// GridRenderer.cpp's (wrong) usage, so this stub build-checked clean
// while the real Arduino IDE failed with "'INKPLATE_BLACK' was not
// declared in this scope" -- there is no such constant in the real
// library. See test/README.md for the full incident.
#define INKPLATE2_WHITE 0
#define INKPLATE2_BLACK 1
#define INKPLATE2_RED 2
#define BLACK 1
#define WHITE 0
#define INKPLATE_1BIT 0
#define INKPLATE_3BIT 1

class Inkplate
{
  public:
    Inkplate() {}
    explicit Inkplate(int /*mode*/) {}

    void begin() {}
    void display() {}
    void clearDisplay() {}

    void setTextSize(int) {}
    void setTextColor(int) {}
    void setTextColor(int, int) {}
    void setCursor(int, int) {}
    void print(const char *) {}
    void print(int) {}

    void getTextBounds(const char *text, int16_t x, int16_t y, int16_t *x1, int16_t *y1, uint16_t *w, uint16_t *h)
    {
        (void)x;
        (void)y;
        *x1 = 0;
        *y1 = 0;
        *w = text ? (uint16_t)(strlen(text) * 6) : 0;
        *h = 8;
    }

    void fillTriangle(int, int, int, int, int, int, int) {}
    void drawRect(int, int, int, int, int) {}

    int width() const { return 600; }
    int height() const { return 448; }
};

#endif // MLB_STUB_INKPLATE_H
