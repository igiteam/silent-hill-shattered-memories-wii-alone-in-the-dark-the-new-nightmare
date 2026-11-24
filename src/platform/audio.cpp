#include "audio.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <vector>

namespace {

constexpr int kRate = 44100;
constexpr int kVoices = 32;

struct Sample {
    std::vector<float> frames;  // mono
};

struct Voice {
    int sound = -1;   // -1: free
    double place = 0; // in frames of the sample
    float pitch = 1;
    float left = 0, right = 0;
    bool loop = false;
    int serial = 0;   // tells voices that reused a slot apart
};

SDL_AudioStream* s_stream = nullptr;
SDL_AudioStream* s_streamAudio = nullptr;  // a video's sound
std::vector<Sample> s_sounds;
Voice s_voices[kVoices];
int s_serial = 0;
std::vector<float> s_mix;

void gains(float volume, float pan, float& left, float& right)
{
    volume = std::clamp(volume, 0.0f, 1.0f);
    pan = std::clamp(pan, 0.0f, 1.0f);
    left = volume * std::min(1.0f, 2.0f * (1.0f - pan));
    right = volume * std::min(1.0f, 2.0f * pan);
}

// SDL asks for more sound: every voice is mixed into it (the stream is
// locked meanwhile)
void SDLCALL feed(void*, SDL_AudioStream* stream, int additional, int)
{
    const int frames = additional / int(2 * sizeof(float));
    if (frames <= 0)
        return;
    s_mix.assign(size_t(frames) * 2, 0.0f);
    for (Voice& v : s_voices) {
        if (v.sound < 0 || size_t(v.sound) >= s_sounds.size()) {
            v.sound = -1;
            continue;
        }
        const std::vector<float>& f = s_sounds[size_t(v.sound)].frames;
        if (f.empty()) {
            v.sound = -1;
            continue;
        }
        for (int k = 0; k < frames; k++) {
            if (v.place >= double(f.size())) {
                if (!v.loop) {
                    v.sound = -1;
                    break;
                }
                v.place -= double(f.size());
            }
            const size_t i = size_t(v.place);
            const float t = float(v.place - double(i));
            const float a = f[i], b = i + 1 < f.size() ? f[i + 1] : (v.loop ? f[0] : 0.0f);
            const float x = a + (b - a) * t;
            s_mix[2 * size_t(k)] += x * v.left;
            s_mix[2 * size_t(k) + 1] += x * v.right;
            v.place += v.pitch;
        }
    }
    for (float& x : s_mix)
        x = std::clamp(x, -1.0f, 1.0f);
    SDL_PutAudioStreamData(stream, s_mix.data(), int(s_mix.size() * sizeof(float)));
}

// A voice number: slot and serial
int voiceNumber(int slot)
{
    return s_voices[slot].serial * kVoices + slot;
}

Voice* voiceOf(int number)
{
    if (number < 0)
        return nullptr;
    Voice& v = s_voices[number % kVoices];
    return v.serial == number / kVoices && v.sound >= 0 ? &v : nullptr;
}

} // namespace

// Opens Windows' default output (SDL follows it if it changes while the
// game runs: headphones, speakers, HDMI...), at full volume
bool openOutput()
{
    if (s_stream)
        SDL_DestroyAudioStream(s_stream);
    s_stream = nullptr;
    for (Voice& v : s_voices)
        v.sound = -1;
    const SDL_AudioSpec spec{ SDL_AUDIO_F32, 2, kRate };
    s_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, feed, nullptr);
    if (!s_stream)
        return false;
    SDL_SetAudioStreamGain(s_stream, 1.0f);  // everything at full volume
    SDL_ResumeAudioStreamDevice(s_stream);
    return true;
}

bool startAudio()
{
    if (s_stream)
        return true;
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO))
        return false;
    return openOutput();
}

std::string audioOutputName()
{
    return s_stream ? "Windows default" : "none";
}

void stopStreamAudio();

void stopAudio()
{
    stopStreamAudio();
    if (s_stream)
        SDL_DestroyAudioStream(s_stream);
    s_stream = nullptr;
    s_sounds.clear();
}

namespace {

// Keeps a sample of any format: to floats, mono, at the mixer's rate
int keepSound(const SDL_AudioSpec& spec, const Uint8* data, int length)
{
    const SDL_AudioSpec want{ SDL_AUDIO_F32, 1, kRate };
    Uint8* converted = nullptr;
    int convertedLength = 0;
    if (!SDL_ConvertAudioSamples(&spec, data, length, &want, &converted, &convertedLength))
        return -1;
    Sample sample;
    sample.frames.resize(size_t(convertedLength) / sizeof(float));
    std::copy_n(reinterpret_cast<const float*>(converted), sample.frames.size(), sample.frames.data());
    SDL_free(converted);
    if (s_stream)
        SDL_LockAudioStream(s_stream);
    s_sounds.push_back(std::move(sample));
    const int number = int(s_sounds.size()) - 1;
    if (s_stream)
        SDL_UnlockAudioStream(s_stream);
    return number;
}

} // namespace

int loadSound(const std::string& path)
{
    SDL_AudioSpec spec;
    Uint8* data = nullptr;
    Uint32 length = 0;
    if (!SDL_LoadWAV(path.c_str(), &spec, &data, &length))
        return -1;
    const int number = keepSound(spec, data, int(length));
    SDL_free(data);
    return number;
}

int loadSoundSamples(const int16_t* samples, int count, int rate)
{
    const SDL_AudioSpec spec{ SDL_AUDIO_S16, 1, rate };
    return count > 0 && rate > 0 ? keepSound(spec, reinterpret_cast<const Uint8*>(samples), count * 2) : -1;
}

int playSound(int sound, float volume, float pan, float pitch, bool loop)
{
    if (!s_stream || sound < 0 || size_t(sound) >= s_sounds.size())
        return -1;
    SDL_LockAudioStream(s_stream);
    int slot = -1;
    for (int k = 0; k < kVoices && slot < 0; k++)
        if (s_voices[k].sound < 0)
            slot = k;
    int number = -1;
    if (slot >= 0) {
        Voice& v = s_voices[slot];
        v.sound = sound;
        v.place = 0;
        v.pitch = std::clamp(pitch, 0.05f, 8.0f);
        v.loop = loop;
        v.serial = ++s_serial & 0xFFFFF;
        gains(volume, pan, v.left, v.right);
        number = voiceNumber(slot);
    }
    SDL_UnlockAudioStream(s_stream);
    return number;
}

bool voicePlaying(int voice)
{
    if (!s_stream)
        return false;
    SDL_LockAudioStream(s_stream);
    const bool playing = voiceOf(voice) != nullptr;
    SDL_UnlockAudioStream(s_stream);
    return playing;
}

void setVoiceVolume(int voice, float volume, float pan)
{
    if (!s_stream)
        return;
    SDL_LockAudioStream(s_stream);
    if (Voice* v = voiceOf(voice))
        gains(volume, pan, v->left, v->right);
    SDL_UnlockAudioStream(s_stream);
}

void stopVoice(int voice)
{
    if (!s_stream)
        return;
    SDL_LockAudioStream(s_stream);
    if (Voice* v = voiceOf(voice))
        v->sound = -1;
    SDL_UnlockAudioStream(s_stream);
}

void stopAllVoices()
{
    if (!s_stream)
        return;
    SDL_LockAudioStream(s_stream);
    for (Voice& v : s_voices)
        v.sound = -1;
    SDL_UnlockAudioStream(s_stream);
}

bool startStreamAudio()
{
    stopStreamAudio();
    const SDL_AudioSpec spec{ SDL_AUDIO_F32, 2, kRate };
    s_streamAudio = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
    if (!s_streamAudio)
        return false;
    SDL_SetAudioStreamGain(s_streamAudio, 1.0f);
    SDL_ResumeAudioStreamDevice(s_streamAudio);
    return true;
}

void putStreamAudio(const float* stereo, int frames)
{
    if (s_streamAudio && frames > 0)
        SDL_PutAudioStreamData(s_streamAudio, stereo, frames * 2 * int(sizeof(float)));
}

int streamAudioWaiting()
{
    return s_streamAudio ? SDL_GetAudioStreamQueued(s_streamAudio) / (2 * int(sizeof(float))) : 0;
}

void stopStreamAudio()
{
    if (s_streamAudio)
        SDL_DestroyAudioStream(s_streamAudio);
    s_streamAudio = nullptr;
}
