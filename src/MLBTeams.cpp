#include "MLBTeams.h"
#include <cstring>

namespace MLBTeams
{

namespace
{
struct TeamIdEntry
{
    const char *abbreviation;
    int id;
};

// All 30 current MLB Stats API team IDs -- see
// https://statsapi.mlb.com/api/v1/teams?sportId=1. A few abbreviations
// diverge from the ESPN/Baseball-Reference codes you might expect (SF
// not SFG, KC not KCR, SD not SDP, TB not TBR, CWS not CHW, AZ not ARI),
// and the Athletics' code changed from OAK to ATH when the franchise
// left Oakland -- this list matches what the schedule endpoint's own
// JSON returns today, not historical codes.
constexpr TeamIdEntry kTeamIds[] = {
    {"ATH", 133}, {"ATL", 144}, {"AZ", 109},  {"BAL", 110}, {"BOS", 111}, {"CHC", 112}, {"CIN", 113}, {"CLE", 114},
    {"COL", 115}, {"CWS", 145}, {"DET", 116}, {"HOU", 117}, {"KC", 118},  {"LAA", 108}, {"LAD", 119}, {"MIA", 146},
    {"MIL", 158}, {"MIN", 142}, {"NYM", 121}, {"NYY", 147}, {"PHI", 143}, {"PIT", 134}, {"SD", 135},  {"SEA", 136},
    {"SF", 137},  {"STL", 138}, {"TB", 139},  {"TEX", 140}, {"TOR", 141}, {"WSH", 120},
};
constexpr size_t kTeamIdCount = sizeof(kTeamIds) / sizeof(kTeamIds[0]);
} // namespace

int lookupTeamId(const char *teamAbbreviation)
{
    if (!teamAbbreviation)
        return 0;

    for (size_t i = 0; i < kTeamIdCount; i++)
    {
        if (strcmp(kTeamIds[i].abbreviation, teamAbbreviation) == 0)
            return kTeamIds[i].id;
    }
    return 0;
}

} // namespace MLBTeams
