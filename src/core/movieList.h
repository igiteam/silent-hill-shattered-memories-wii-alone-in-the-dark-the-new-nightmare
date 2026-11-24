#pragma once

// The list of the game's videos: MOVIES.TXT in the game's folder (read by
// FUN_00480ee0), a line each: number, name (movies\<name>.BIK), and two
// values (the game keeps the last one, & 0x7FFF, for its player: not
// studied). The game plays them by number (FUN_004811cf): 0 the logos and 56
// Spiral House's at start, 3 the trailer, 2 the intro at a new game; the
// scripts others (function 42).

#include <map>
#include <string>

// Reads MOVIES.TXT of the data folder: name by number. Empty if missing
std::map<int, std::string> loadMovieList(const std::string& dataFolder);
