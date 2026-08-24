#ifndef GRID_RENDERER_H
#define GRID_RENDERER_H

#include "ScoreRenderer.h"

// Multi-game layout for the larger Inkplate boards (6, 6PLUS, 10, etc).
// Lays games out in a grid of cards sized off of display.width()/height()
// at render time, so the same renderer works across every board in that
// family without per-model tuning. Uses 1-bit grayscale color constants
// (INKPLATE_BLACK/INKPLATE_WHITE) which are valid on every non-Inkplate2
// board; boards with true color (6COLOR) will just render in black/white,
// which is a deliberate simplification for v0.1 -- see README.
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
