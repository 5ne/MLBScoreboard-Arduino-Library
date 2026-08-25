#ifndef MLB_TEAMS_H
#define MLB_TEAMS_H

// Maps the human-friendly team abbreviations this library takes
// everywhere else ("SEA", "NYY", ...) to the MLB Stats API's numeric
// team IDs, needed to ask the schedule endpoint to filter server-side
// (?teamId=136) instead of returning every game league-wide for the
// date. That cuts the response MLBDataSource has to hold in RAM while
// parsing from "today's ~15 games" down to "this one team's game" --
// see MLBDataSource.cpp's use of this and the README's "Data source"
// section for why response size matters here.
//
// Pure lookup, no network/hardware dependency -- unit tested in
// test/test_teams.cpp the same way MLBParsing is.
namespace MLBTeams
{

// Looks up the MLB Stats API numeric team ID for a team abbreviation
// (case-sensitive, matching the same abbreviations MLBParsing matches
// against the API's own JSON -- see the reference list in README.md).
// Returns 0 if the abbreviation isn't recognized, so callers can fall
// back to an unfiltered request rather than silently sending a bogus
// teamId.
int lookupTeamId(const char *teamAbbreviation);

} // namespace MLBTeams

#endif // MLB_TEAMS_H
