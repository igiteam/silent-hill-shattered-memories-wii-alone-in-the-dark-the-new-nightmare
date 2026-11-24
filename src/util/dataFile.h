#pragma once

// Reading whole files, unpacking the gzip ones (most data files of the game
// are gzip, padded with zeros to 2048 bytes, the size of a CD sector).

#include <cstdint>
#include <string>
#include <vector>

// Contents of a file, unpacked if it is gzip. False if it cannot be read or
// its gzip is damaged
bool readDataFile(const std::string& path, std::vector<uint8_t>& out);

// Unscrambles the game's .csv tables (NpcCarac.csv, cweap.csv, aweap.csv;
// FUN_00479cd2): each byte xor its place (mod 256), then its two low bits go
// to the top and its two top bits to the bottom
void unscrambleTable(std::vector<uint8_t>& data);
