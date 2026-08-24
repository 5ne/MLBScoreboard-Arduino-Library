#ifndef COMPACT_RENDERER_H
#define COMPACT_RENDERER_H

#include "ScoreRenderer.h"

// Single-game layout for the Inkplate 2: 212x104px, 3 colors
// (INKPLATE2_WHITE / INKPLATE2_BLACK / INKPLATE2_RED). Only ever draws
// games[0] -- the Inkplate 2 doesn't have the resolution for more than
// one game at a time. Red is reserved for the "live" state and for the
// trailing team's score, since that's the one accent this panel has.
class CompactRenderer : public ScoreRenderer
{
  public:
    void render(Inkplate &display, const MLBGame *games, int count, int favoriteTeamIndex = -1) override;

  private:
    void renderPreview(Inkplate &display, const MLBGame &g);
    void renderLive(Inkplate &display, const MLBGame &g);
    void renderFinal(Inkplate &display, const MLBGame &g);
    void renderNoGame(Inkplate &display);
    void renderStaleMarker(Inkplate &display);
};

#endif // COMPACT_RENDERER_H
