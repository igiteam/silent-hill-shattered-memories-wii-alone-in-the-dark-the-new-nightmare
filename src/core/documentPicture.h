#pragma once

// The full-screen pictures of the game: doc\*.VRM (close views of puzzles and
// things examined) and book\*.VRM (pages). Made with the Dreamcast tool
// DcBitmap: 96 bytes of header (at 0x40 the sheet's width and height, at 0x46
// the picture's) and the sheet, 16 bits a pixel row after row, colour 5-5-5
// (docs/FORMATOS.md). The scripts show them with functions 8 and 63.

#include <cstdint>
#include <string>
#include <vector>

struct DocumentPicture {
    int width = 0, height = 0;    // of the picture (640 x 480)
    std::vector<uint32_t> rgba;   // width x height colours 0xAABBGGRR
};

// Reads a picture by the name the scripts give ("\doc\101_51_A"), from the
// data folder (.VRM added). False if it is missing or not one
bool loadDocumentPicture(const std::string& dataFolder, const std::string& name, DocumentPicture& out);
