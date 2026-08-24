#ifndef SCORE_RENDERER_H
#define SCORE_RENDERER_H

#include <Inkplate.h>
#include "../MLBGame.h"

// One renderer implementation per *display class*, not per board model --
// e.g. every 3-color 212x104 panel (just the Inkplate 2 today) shares
// CompactRenderer; every larger grayscale/color panel shares GridRenderer.
// Renderers only draw; they never touch WiFi or the network.
class ScoreRenderer
{
  public:
    virtual ~ScoreRenderer() = default;

    // Draws `count` games (1 for CompactRenderer, which ignores anything
    // past games[0]) onto `display`. Does NOT call display.display() --
    // the caller decides when to flush, so it can batch multiple draws
    // or choose partial vs full refresh.
    virtual void render(Inkplate &display, const MLBGame *games, int count, int favoriteTeamIndex = -1) = 0;
};

#endif // SCORE_RENDERER_H
