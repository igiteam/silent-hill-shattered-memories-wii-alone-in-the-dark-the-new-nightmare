#pragma once

// The game's videos (movies\*.BIK: Bink 1, 640 x 320 at 30 frames a second,
// with sound), read with FFmpeg (ThirdParty/ffmpeg, LGPL). Their sound goes
// out as it is decoded (platform/audio: stream), their pictures are given
// when their time comes. One video at a time. Without FFmpeg (a build
// without it) no video opens.

#include <cstdint>
#include <string>
#include <vector>

// Opens a video and starts its sound. False if it cannot be read
bool openVideo(const std::string& path);

// The picture to show after that many milliseconds of play: true if there is
// a new one (width x height colours 0xAABBGGRR in rgba)
bool videoPicture(double milliseconds, int& width, int& height, std::vector<uint32_t>& rgba);

// Whether it has shown all its pictures
bool videoFinished();

// Stops it (and its sound)
void closeVideo();
