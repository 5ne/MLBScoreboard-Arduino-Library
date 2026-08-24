#ifndef GRID_RENDERER_H
#define GRID_RENDERER_H

#include "ScoreRenderer.h"

// Multi-game layout for the larger Inkplate boards (6, 6PLUS, 10, etc).
// Lays games out in a grid of cards sized off of display.width()/height()
// at render time, so the same renderer works across every board in that
// family without per-model tuning. Uses the plain Adafruit_GFX-style
// `BLACK`/`WHITE` constants the Inkplate library defines for its
// grayscale boards (see src/system/defines.h in the Inkplate library --
// NOT `INKPLATE_BLACK`/`INKPLATE_WHITE`, which only exist for the 7-color
// 6COLOR/13" Spectra boards and won't compile against a grayscale board
// selection). Those true-color boards will just render in black/white
// here, which is a deliberate simplification for v0.1 -- see README.
class GridRenderer : public ScoreRenderer
{
  public:
    // columns <= 0 means "pick automatically based on game count and
    // screen width".
    explicit GridRenderer(int columns = 0);

    void render(Inkplate &display, const MLBGame *games, int count, int favoriteTeamIndex = -1) override;

  private:
    int _columns;

    void renderCard(Inkplate &display, const MLBGame &g, int x, int y, int w, int h, bool isFavorite);
};

#endif // GRID_RENDERER_H
