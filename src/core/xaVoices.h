#pragma once

// The voices of the radio scenes (FUN_00480e32, "PlayXa %d"): Xa.txt in the
// game's folder (scrambled as the .csv tables) gives for each voice number a
// line "file,entry": the entry of xa\XaNN.xa, an AFS archive ("AFS\0", how
// many entries, then offset and size of each) whose entries are CRI ADX
// sounds (mono, 22050 a second, 4 bits a sample).

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

class XaVoices {
public:
    // Reads Xa.txt of the data folder. False if it is missing
    bool load(const std::string& dataFolder);

    // How many voices there are
    int count() const { return int(places_.size()); }

    // The sound of a voice: 16-bit samples, mono, and their rate. False if it
    // cannot be read
    bool read(int voice, std::vector<int16_t>& samples, int& rate) const;

private:
    std::string folder_;
    std::vector<std::pair<int, int>> places_;  // file, entry
};

// Decodes a CRI ADX sound (encoding 3: frames of a 16-bit scale and 32
// samples of 4 bits, predicted from the two before with coefficients from
// its high-pass frequency), the first channel only. False if it is not one
bool decodeAdx(const uint8_t* data, size_t size, std::vector<int16_t>& samples, int& rate);
