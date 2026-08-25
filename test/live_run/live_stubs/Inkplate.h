#ifndef MLB_LIVE_STUB_INKPLATE_H
#define MLB_LIVE_STUB_INKPLATE_H

// Stand-in for SolderedElectronics/Inkplate-Arduino-library, for
// test/live_run only (see Arduino.h in this directory). There's no
// physical Inkplate here to draw to, so instead of a silent no-op (like
// test/arduino_stubs/Inkplate.h), every draw call PRINTS what it would
// have drawn -- position, color, text -- so you can read the renderer's
// actual output as a trace instead of guessing from the e-paper.
//
// Color constant NAMES are verified against the real library (see the
// comment in test/arduino_stubs/Inkplate.h for exactly which upstream
// files). The Inkplate 2's INKPLATE2_* numeric values specifically
// weren't independently confirmed against upstream source (only that
// the names themselves are real, via a real Inkplate2 example sketch) --
// doesn't matter here, since this stub never touches real hardware and
// this harness is for debugging data/parsing, not pixel-exact color.

#include <cstdint>
#include <cstring>
#include <cstdio>

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
    Inkplate() { printf("[DISPLAY] Inkplate() -- Inkplate 2, 3-color mode\n"); }
    explicit Inkplate(int mode) { printf("[DISPLAY] Inkplate(mode=%d)\n", mode); }

    void begin() { printf("[DISPLAY] begin()\n"); }
    void display() { printf("[DISPLAY] display()  <-- flush to e-paper happens here\n"); }
    void clearDisplay() { printf("[DISPLAY] clearDisplay()\n"); }

    void setTextSize(int s) { printf("[DISPLAY] setTextSize(%d)\n", s); }
    void setTextColor(int c) { printf("[DISPLAY] setTextColor(%s)\n", colorName(c)); }
    void setTextColor(int c, int bg) { printf("[DISPLAY] setTextColor(%s, bg=%s)\n", colorName(c), colorName(bg)); }
    void setCursor(int x, int y) { printf("[DISPLAY] setCursor(%d, %d)\n", x, y); }
    void print(const char *s) { printf("[DISPLAY] print(\"%s\")\n", s); }
    void print(int v) { printf("[DISPLAY] print(%d)\n", v); }

    void getTextBounds(const char *text, int16_t x, int16_t y, int16_t *x1, int16_t *y1, uint16_t *w, uint16_t *h)
    {
        (void)x;
        (void)y;
        *x1 = 0;
        *y1 = 0;
        // Same rough 6px/char approximation as test/arduino_stubs/Inkplate.h --
        // fine for eyeballing layout, not a real font metrics engine.
        *w = text ? (uint16_t)(strlen(text) * 6) : 0;
        *h = 8;
    }

    void fillTriangle(int x0, int y0, int x1, int y1, int x2, int y2, int color)
    {
        printf("[DISPLAY] fillTriangle((%d,%d),(%d,%d),(%d,%d), %s)\n", x0, y0, x1, y1, x2, y2, colorName(color));
    }

    void drawRect(int x, int y, int w, int h, int color)
    {
        printf("[DISPLAY] drawRect(x=%d, y=%d, w=%d, h=%d, %s)\n", x, y, w, h, colorName(color));
    }

    int width() const { return 212; }  // Inkplate 2 dimensions
    int height() const { return 104; }

  private:
    static const char *colorName(int c)
    {
        switch (c)
        {
        case 0:
            return "WHITE(0)";
        case 1:
            return "BLACK(1)";
        case 2:
            return "RED(2)";
        default:
            return "?";
        }
    }
};

#endif // MLB_LIVE_STUB_INKPLATE_H
