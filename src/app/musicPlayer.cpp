#include "musicPlayer.h"

#include <algorithm>
#include <cmath>

#include "audio.h"

namespace {

constexpr int kMostVoices = 12;  // FUN_0049b5b6 looks among 12 samples
// A note let go fades out (FUN_0049b7ea starts a timer that lowers it; its
// rate is not studied: ours, about a tenth of a second)
constexpr float kReleaseMilliseconds = 100;

// The frequency of a MIDI note (the game's table at 0x4E7F68: 8.18 Hz for
// note 0, doubling every 12)
float noteFrequency(int note)
{
    return 8.1757989f * std::pow(2.0f, float(std::clamp(note, 0, 127)) / 12.0f);
}

} // namespace

void MusicPlayer::setup(const SoundBanks* banks, std::function<int(const std::string&)> sampleNamed)
{
    banks_ = banks;
    sampleNamed_ = std::move(sampleNamed);
}

bool MusicPlayer::loadFile(const std::string& dataFolder, const std::string& name)
{
    MusicFile file;
    if (!loadMusicFile(dataFolder, name, file))
        return false;
    for (size_t k = 0; k < files_.size(); k++)
        if (files_[k].bank == file.bank || files_[k].name == file.name) {
            // The slots playing from the file replaced stop (their sequence is gone)
            for (int s = 0; s < kSlots; s++)
                if (slots_[s].file == int(k))
                    stop(s);
            files_[k] = std::move(file);
            return true;
        }
    files_.push_back(std::move(file));
    return true;
}

bool MusicPlayer::play(int slot, const std::string& sequence, int left, int right, int section, int repeats)
{
    if (slot < 0 || slot >= kSlots)
        return false;
    for (size_t f = 0; f < files_.size(); f++)
        for (const MusicSequence& s : files_[f].sequences)
            if (s.name == sequence) {
                Slot& sl = slots_[slot];
                if (sl.player.playing && sl.player.sequence == &s) {
                    setVolume(slot, left, right);
                    return true;
                }
                stop(slot);
                startSequence(sl.player, s, left, right, section, repeats);
                sl.file = int(f);
                return true;
            }
    return false;
}

void MusicPlayer::stop(int slot)
{
    if (slot < 0 || slot >= kSlots)
        return;
    slots_[slot].player.playing = false;
    for (Voice& v : voices_)
        if (v.slot == slot)
            endVoice(v, false);
}

void MusicPlayer::setVolume(int slot, int left, int right)
{
    if (slot < 0 || slot >= kSlots || !slots_[slot].player.playing)
        return;
    SequencePlayer& p = slots_[slot].player;
    p.left = std::clamp(left, 0, 127) << 7;
    p.right = std::clamp(right, 0, 127) << 7;
    p.leftTarget = p.rightTarget = -1;
    for (int c = 0; c < 16; c++) {
        apply(slot, { MusicEvent::Volume, c });
        apply(slot, { MusicEvent::Pan, c });
    }
}

void MusicPlayer::setSection(int slot, int section, int repeats)
{
    if (slot < 0 || slot >= kSlots || !slots_[slot].player.playing || !slots_[slot].player.sequence)
        return;
    if (section >= 0 && size_t(section) < slots_[slot].player.sequence->marks.size() / 2) {
        slots_[slot].player.section = section;
        slots_[slot].player.repeats = repeats;
    }
}

void MusicPlayer::fade(int slot, int left, int right, int seconds)
{
    if (slot >= 0 && slot < 5 && slots_[slot].player.playing)
        fadeSequence(slots_[slot].player, left, right, seconds);
}

void MusicPlayer::stopAll()
{
    for (int s = 0; s < kSlots; s++)
        stop(s);
}

float MusicPlayer::voiceVolume(int slot, int channel) const
{
    const SequencePlayer& p = slots_[slot].player;
    // FUN_0049ad30: channel volume x strength (127) / 127 x the sequence's / 127
    return float(p.volume[channel]) / 127.0f * float(sequenceVolume(p)) / 127.0f;
}

float MusicPlayer::voicePan(int slot, int channel) const
{
    const SequencePlayer& p = slots_[slot].player;
    return float(std::clamp(p.pan[channel] + sequencePan(p) - 64, 0, 127)) / 127.0f;
}

void MusicPlayer::endVoice(Voice& v, bool fadeOut)
{
    if (v.voice < 0)
        return;
    if (fadeOut && v.release < 0) {
        v.release = 0;
        return;
    }
    stopVoice(v.voice);
    v.voice = -1;
}

void MusicPlayer::apply(int slot, const MusicEvent& e)
{
    const SequencePlayer& p = slots_[slot].player;
    switch (e.kind) {
    case MusicEvent::NoteOn: {
        // One note a channel: the one before stops
        for (Voice& v : voices_)
            if (v.slot == slot && v.channel == e.channel && v.release < 0)
                endVoice(v, true);
        voices_.erase(std::remove_if(voices_.begin(), voices_.end(), [](const Voice& v) { return v.voice < 0; }),
                      voices_.end());
        if (!banks_ || !sampleNamed_ || slots_[slot].file < 0 || int(voices_.size()) >= kMostVoices)
            return;
        const MusicFile& file = files_[size_t(slots_[slot].file)];
        const int program = std::clamp(e.program, 0, 127);
        const auto kinds = banks_->musicKinds.find(file.name);
        const bool ambient = kinds != banks_->musicKinds.end() && size_t(program) < kinds->second.size()
                             && kinds->second[size_t(program)] == 2;
        const MusicInstrument& instrument = ambient ? banks_->ambient[program] : banks_->musical[program];
        if (instrument.sample.empty() || instrument.rootFrequency <= 0)
            return;
        const int sample = sampleNamed_(instrument.sample);
        if (sample < 0)
            return;
        // FUN_0049b5b6: 44100 x the note's frequency x 100 / the root's
        const float pitch = noteFrequency(e.note) * 100.0f / instrument.rootFrequency;
        const float volume = voiceVolume(slot, e.channel);
        const int voice = playSound(sample, volume, voicePan(slot, e.channel), pitch, instrument.loops);
        if (voice >= 0)
            voices_.push_back({ voice, slot, e.channel, volume, -1 });
        break;
    }
    case MusicEvent::NoteOff:
        for (Voice& v : voices_)
            if (v.slot == slot && v.channel == e.channel && v.release < 0)
                endVoice(v, true);
        break;
    case MusicEvent::Volume:
    case MusicEvent::Pan:
        // The notes of the channel playing follow (FUN_0049b906, FUN_0049b988)
        for (Voice& v : voices_)
            if (v.slot == slot && v.channel == e.channel && v.voice >= 0 && v.release < 0) {
                v.volume = voiceVolume(slot, e.channel);
                setVoiceVolume(v.voice, v.volume, voicePan(slot, e.channel));
            }
        break;
    case MusicEvent::Stop:
        for (Voice& v : voices_)
            if (v.slot == slot)
                endVoice(v, true);
        break;
    }
    (void)p;
}

void MusicPlayer::update(float milliseconds)
{
    clock_ += milliseconds;
    std::vector<MusicEvent> events;
    for (int guard = 0; clock_ >= kMusicStepMilliseconds && guard < 100; guard++) {
        clock_ -= kMusicStepMilliseconds;
        for (int s = 0; s < kSlots; s++) {
            Slot& sl = slots_[s];
            if (!sl.player.playing)
                continue;
            const int left = sl.player.left, right = sl.player.right;
            events.clear();
            stepSequence(sl.player, events);
            for (const MusicEvent& e : events)
                apply(s, e);
            if (sl.player.left != left || sl.player.right != right)
                for (int c = 0; c < 16; c++)
                    apply(s, { MusicEvent::Volume, c });  // a fade moved its volume
        }
    }
    clock_ = std::min(clock_, kMusicStepMilliseconds);
    // The notes let go fade out; the ones over are forgotten
    for (Voice& v : voices_) {
        if (v.voice >= 0 && !voicePlaying(v.voice))
            v.voice = -1;
        if (v.voice >= 0 && v.release >= 0) {
            v.release += milliseconds;
            if (v.release >= kReleaseMilliseconds)
                endVoice(v, false);
            else
                setVoiceVolume(v.voice, v.volume * (1.0f - v.release / kReleaseMilliseconds),
                               v.slot >= 0 ? voicePan(v.slot, v.channel) : 0.5f);
        }
    }
    voices_.erase(std::remove_if(voices_.begin(), voices_.end(), [](const Voice& v) { return v.voice < 0; }),
                  voices_.end());
}
