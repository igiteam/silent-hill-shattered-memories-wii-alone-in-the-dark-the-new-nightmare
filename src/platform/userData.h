#pragma once

// Where the program keeps what it writes (settings...): the folder UserData
// next to its executable, made if missing. Never the game's folder

#include <string>

// The folder (with its last slash); empty if it cannot be made
std::string userDataFolder();

// A small text file of that folder (a setting): its contents, or empty
std::string readUserText(const std::string& name);
// Writes it; false if it cannot
bool writeUserText(const std::string& name, const std::string& text);
