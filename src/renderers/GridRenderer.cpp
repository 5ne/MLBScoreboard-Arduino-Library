#include "GridRenderer.h"
#include <math.h>
#include <cstdio>
#include <cstring>

GridRenderer::GridRenderer(int columns) : _columns(columns)
{
}

void GridRenderer::render(Inkplate &display, const MLBGame *games, int count, int favoriteTeamIndex)
{
    display.clearDisplay();

    if (count <= 0)
    {
        display.setTextSize(2);
        display.setTextColor(BLACK);
        display.setCursor(20, 20);
        display.print("No games today");
        return;
    }

    int screenW = display.width();
    int screenH = display.height();

    int cols = _columns > 0 ? _columns : (int)ceil(sqrt((double)count));
    if (cols < 1)
        cols = 1;
    int rows = (int)ceil((double)count / cols);

    int cardW = screenW / cols;
    int cardH = screenH / rows;

    for (int i = 0; i < count; i++)
    {
        int col = i % cols;
        int row = i / cols;
        renderCard(display, games[i], col * cardW, row * cardH, cardW, cardH, i == favoriteTeamIndex);
    }
}

void GridRenderer::renderCard(Inkplate &display, const MLBGame &g, int x, int y, int w, int h, bool isFavorite)
{
    const int pad = 6;

    // Favorite team gets a border so it stands out on a busy grid --
    // this is the one accent available uniformly across grayscale boards.
    if (isFavorite)
        display.drawRect(x + 1, y + 1, w - 2, h - 2, BLACK);

    if (!g.isValid)
    {
        display.setTextSize(1);
        display.setTextColor(BLACK);
        display.setCursor(x + pad, y + pad);
        display.print("No data");
        return;
    }

    char matchup[24];
    snprintf(matchup, sizeof(matchup), "%s @ %s", g.awayTeam, g.homeTeam);
    display.setTextSize(1);
    display.setTextColor(BLACK);
    display.setCursor(x + pad, y + pad);
    display.print(matchup);

    char scoreLine[24];
    const char *tag = "";
    switch (g.state)
    {
    case GAME_STATE_PREVIEW:
        snprintf(scoreLine, sizeof(scoreLine), "%s", g.startTimeLocal[0] ? g.startTimeLocal : "TBD");
        break;
    case GAME_STATE_LIVE: {
        const char *half = g.inningTopHalf ? "T" : "B";
        snprintf(scoreLine, sizeof(scoreLine), "%d-%d  %s%d", g.awayScore, g.homeScore, half, g.inning);
        tag = "LIVE";
        break;
    }
    case GAME_STATE_FINAL:
        snprintf(scoreLine, sizeof(scoreLine), "%d-%d", g.awayScore, g.homeScore);
        tag = "F";
        break;
    default:
        snprintf(scoreLine, sizeof(scoreLine), "--");
        break;
    }

    display.setTextSize(2);
    display.setCursor(x + pad, y + pad + 14);
    display.print(scoreLine);

    if (tag[0] != '\0')
    {
        display.setTextSize(1);
        display.setCursor(x + w - pad - (int)strlen(tag) * 6, y + pad);
        display.print(tag);
    }

    if (g.isStale)
    {
        display.setTextSize(1);
        display.setCursor(x + pad, y + h - pad - 8);
        display.print("(stale)");
    }
}
