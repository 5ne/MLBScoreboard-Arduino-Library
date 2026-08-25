#include "CompactRenderer.h"
#include <cstdio>

namespace
{
constexpr int kScreenW = 212;
constexpr int kScreenH = 104;

void drawCenteredText(Inkplate &display, const char *text, int y, int textSize, int color)
{
    int16_t x1, y1;
    uint16_t w, h;
    display.setTextSize(textSize);
    display.setTextColor(color);
    display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
    int x = (kScreenW - (int)w) / 2;
    if (x < 0)
        x = 0;
    display.setCursor(x, y);
    display.print(text);
}
} // namespace

void CompactRenderer::render(Inkplate &display, const MLBGame *games, int count, int favoriteTeamIndex)
{
    // The Inkplate 2 only ever shows one game (games[0]), so there's no
    // "which card is the favorite" distinction to draw -- unlike
    // GridRenderer, which uses this to outline the favorite team's card.
    (void)favoriteTeamIndex;

    display.clearDisplay();

    if (count <= 0 || !games[0].isValid)
    {
        // isValid can be false either because the team has no game today,
        // or because the last fetch failed. We can't tell the difference
        // here without more context, so the caller (MLBScoreboard) is
        // responsible for calling renderStale() explicitly when it knows
        // it's showing cached data after a failed refresh.
        renderNoGame(display);
        return;
    }

    const MLBGame &g = games[0];
    switch (g.state)
    {
    case GAME_STATE_PREVIEW:
        renderPreview(display, g);
        break;
    case GAME_STATE_LIVE:
        renderLive(display, g);
        break;
    case GAME_STATE_FINAL:
        renderFinal(display, g);
        break;
    default:
        renderNoGame(display);
        break;
    }

    if (g.isStale)
        renderStaleMarker(display);
}

void CompactRenderer::renderNoGame(Inkplate &display)
{
    drawCenteredText(display, "No games today!!", 46, 2, INKPLATE2_BLACK);
}

void CompactRenderer::renderStaleMarker(Inkplate &display)
{
    // Small "!" flag in the corner so the viewer knows this is cached
    // data from a failed refresh, not a fresh score. Drawn on top of
    // whatever state render() already produced.
    display.fillTriangle(kScreenW - 14, 4, kScreenW - 4, 4, kScreenW - 9, 14, INKPLATE2_RED);
    display.setTextColor(INKPLATE2_WHITE);
    display.setTextSize(1);
    display.setCursor(kScreenW - 11, 5);
    display.print("!");
}

void CompactRenderer::renderPreview(Inkplate &display, const MLBGame &g)
{
    char matchup[24];
    snprintf(matchup, sizeof(matchup), "%s @ %s", g.awayTeam, g.homeTeam);
    drawCenteredText(display, matchup, 20, 2, INKPLATE2_BLACK);

    char timeLine[24];
    if (g.startTimeLocal[0] != '\0')
        snprintf(timeLine, sizeof(timeLine), "Today %s", g.startTimeLocal);
    else
        snprintf(timeLine, sizeof(timeLine), "Today");
    drawCenteredText(display, timeLine, 55, 2, INKPLATE2_BLACK);
}

void CompactRenderer::renderLive(Inkplate &display, const MLBGame &g)
{
    // Top row: AWAY score  -  HOME score, trailing team's number in red.
    char awayScore[8], homeScore[8];
    snprintf(awayScore, sizeof(awayScore), "%d", g.awayScore);
    snprintf(homeScore, sizeof(homeScore), "%d", g.homeScore);

    int awayColor = (g.awayScore < g.homeScore) ? INKPLATE2_RED : INKPLATE2_BLACK;
    int homeColor = (g.homeScore < g.awayScore) ? INKPLATE2_RED : INKPLATE2_BLACK;

    display.setTextSize(4);
    display.setTextColor(awayColor);
    display.setCursor(30, 10);
    display.print(awayScore);

    display.setTextColor(INKPLATE2_BLACK);
    display.setCursor(96, 10);
    display.print("-");

    display.setTextColor(homeColor);
    display.setCursor(120, 10);
    display.print(homeScore);

    // Team abbreviations under each score.
    display.setTextSize(1);
    display.setTextColor(INKPLATE2_BLACK);
    display.setCursor(30, 55);
    display.print(g.awayTeam);
    display.setCursor(120, 55);
    display.print(g.homeTeam);

    // Inning/count line, e.g. "TOP 5 - 1 OUT"
    char inningLine[32];
    const char *half = g.inningTopHalf ? "TOP" : "BOT";
    const char *outWord = (g.outs == 1) ? "OUT" : "OUTS";
    snprintf(inningLine, sizeof(inningLine), "%s %d - %d %s", half, g.inning, g.outs, outWord);
    drawCenteredText(display, inningLine, 75, 1, INKPLATE2_BLACK);

    // Small "LIVE" tag in red, top-left.
    display.setTextSize(1);
    display.setTextColor(INKPLATE2_RED);
    display.setCursor(2, 2);
    display.print("LIVE");
}

void CompactRenderer::renderFinal(Inkplate &display, const MLBGame &g)
{
    char awayLine[24], homeLine[24];
    snprintf(awayLine, sizeof(awayLine), "%s  %d", g.awayTeam, g.awayScore);
    snprintf(homeLine, sizeof(homeLine), "%s  %d", g.homeTeam, g.homeScore);

    int awayColor = (g.awayScore > g.homeScore) ? INKPLATE2_BLACK : INKPLATE2_RED;
    int homeColor = (g.homeScore > g.awayScore) ? INKPLATE2_BLACK : INKPLATE2_RED;
    // Winner in black (bold/normal), loser in red -- inverse of renderLive,
    // since "red" here means "lost" rather than "currently behind".

    drawCenteredText(display, "FINAL", 8, 1, INKPLATE2_BLACK);

    display.setTextSize(3);
    display.setTextColor(awayColor);
    display.setCursor(20, 30);
    display.print(awayLine);

    display.setTextColor(homeColor);
    display.setCursor(20, 65);
    display.print(homeLine);
}
