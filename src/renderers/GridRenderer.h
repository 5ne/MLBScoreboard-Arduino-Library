#ifndef GRID_RENDERER_H
#define GRID_RENDERER_H

#include "ScoreRenderer.h"

// Multi-game layout for the larger Inkplate boards (6, 6PLUS, 10, etc).
// Lays games out in a grid of cards sized off of display.width()/height()
// at render time, so the same renderer works across every board in that
// family without per-model tuning. Uses the plain BLACK/WHITE color
// constants that SolderedElectronics/Inkplate-Arduino-library's
// system/defines.h defines for every board that isn't Inkplate 2,
// Inkplate 6COLOR, or Inkplate 13 SPECTRA (those get their own
// swapped/multi-color constants, e.g. INKPLATE2_BLACK -- see
// CompactRenderer). There is no "INKPLATE_BLACK"/"INKPLATE_WHITE" in the
// real library -- an earlier version of this file invented those names
// and they only ever compiled by accident; see test/README.md for the
// regression this caused and how the test suite now guards against it.
// True-color boards (6COLOR) will just render in black/white here, which
// is a deliberate simplification for v0.1 -- see README.
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
