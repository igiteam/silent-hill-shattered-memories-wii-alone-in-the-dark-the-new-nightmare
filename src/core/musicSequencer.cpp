#include "musicSequencer.h"

#include <algorithm>

namespace {

constexpr int kStepsPerSecond = 200;  // the game's timer (DAT_004cdc00)

// Goes to a mark of the sequence (the delta of the event there)
void jumpTo(SequencePlayer& p, uint32_t mark)
{
    if (mark < p.sequence->data.size())
        p.position = mark;
}

// The volume of a side moves a step towards its target, as FUN_004987f2
void rampSide(int& value, int& target, int& step)
{
    if (target < 0)
        return;
    if (step > 0) {
        if (value < target)
            value = std::min(value + step, 127 << 7);
        else {
            target = -1;
            step = 0;
        }
    } else if (step < 0) {
        if (target < value && value + step > 0)
            value += step;
        else {
            target = -1;
            step = 0;
        }
    }
}

} // namespace

void startSequence(SequencePlayer& p, const MusicSequence& sequence, int left, int right, int section, int repeats)
{
    p = SequencePlayer();
    p.sequence = &sequence;
    p.left = (left & 0xFF) << 7;
    p.right = (right & 0xFF) << 7;
    p.section = section;
    p.repeats = repeats;
    for (int c = 0; c < 16; c++) {
        p.volume[c] = 0x7F;
        p.pan[c] = 0x40;
    }
    p.position = sequence.events;
    p.playing = true;
}

int sequenceVolume(const SequencePlayer& p)
{
    return ((p.left >> 7) + (p.right >> 7)) / 2;
}

int sequencePan(const SequencePlayer& p)
{
    const int sum = (p.left >> 7) + (p.right >> 7);
    return sum ? std::clamp(((p.right >> 7) * 128 - 1) / sum, 0, 127) : 64;
}

void fadeSequence(SequencePlayer& p, int left, int right, int seconds)
{
    left = std::clamp(left, 0, 127);
    right = std::clamp(right, 0, 127);
    p.leftTarget = left << 7;
    p.rightTarget = right << 7;
    const int steps = std::max(seconds, 1) * kStepsPerSecond;
    p.leftStep = ((left << 7) - p.left) / steps;
    p.rightStep = ((right << 7) - p.right) / steps;
}

void stepSequence(SequencePlayer& p, std::vector<MusicEvent>& out)
{
    if (!p.playing || !p.sequence)
        return;
    rampSide(p.left, p.leftTarget, p.leftStep);
    rampSide(p.right, p.rightTarget, p.rightStep);
    const std::vector<uint8_t>& d = p.sequence->data;
    p.clock += int64_t(p.sequence->speed / kStepsPerSecond);
    for (int guard = 0; guard < 10000 && p.playing; guard++) {
        // The delta of the next event (variable length)
        size_t at = p.position;
        if (at >= d.size()) {
            p.playing = false;
            out.push_back({ MusicEvent::Stop });
            return;
        }
        int64_t delta = d[at] & 0x7F;
        while ((d[at] & 0x80) && at + 1 < d.size()) {
            at++;
            delta = delta << 7 | (d[at] & 0x7F);
        }
        if (p.clock < delta * 1000)
            return;  // not yet
        p.clock -= delta * 1000;
        const size_t e = at + 1;  // the event
        if (e >= d.size()) {
            p.playing = false;
            return;
        }
        const uint8_t status = d[e];
        const int channel = status & 0x0F;
        auto byte = [&](size_t k) { return e + k < d.size() ? d[e + k] : 0; };
        size_t next = e + 1;
        switch (status & 0xF0) {
        case 0x80:  // the channel's note stops (FUN_00499d06)
            out.push_back({ MusicEvent::NoteOff, channel });
            next = e + 2;
            break;
        case 0x90:  // a note, an octave down (FUN_00499d46)
            out.push_back({ MusicEvent::NoteOn, channel, byte(1) - 12, p.program[channel] });
            next = e + 3;
            break;
        case 0xB0:
            next = e + 3;
            switch (byte(1)) {
            case 7:
                p.volume[channel] = byte(2);
                out.push_back({ MusicEvent::Volume, channel });
                break;
            case 10:
                p.pan[channel] = byte(2);
                out.push_back({ MusicEvent::Pan, channel });
                break;
            case 0x62: {  // a mark of a section
                const int v = byte(2);
                const std::vector<uint32_t>& marks = p.sequence->marks;
                auto mark = [&](size_t k) { return k < marks.size() ? marks[k] : uint32_t(d.size()); };
                const size_t entry = size_t(p.section) * 2;
                if (v < 0x40) {
                    if (p.section != v) {
                        if (p.section == v - 1) {
                            // The end of the section playing: back to its loop
                            if (p.repeats == 0 || --p.repeats != 0) {
                                jumpTo(p, mark(entry + 1));
                                continue;
                            }
                            p.playing = false;
                            out.push_back({ MusicEvent::Stop });
                            return;
                        }
                        jumpTo(p, mark(entry));
                        continue;
                    }
                } else if (p.section != v - 0x40) {
                    jumpTo(p, mark(entry));
                    continue;
                }
                break;
            }
            case 0x67:
            case 0x69:
                next = e + 4;  // two more bytes (0x67: FUN_00499d8e, not done)
                break;
            default:
                break;
            }
            break;
        case 0xC0:
            p.program[channel] = byte(1);
            next = e + 2;
            break;
        case 0xE0:  // pitch bend: kept by the game but never heard (only a note's start sets its rate)
            next = e + 3;
            break;
        default:
            if (status == 0xFF) {
                if (byte(1) == 0x2F) {  // the end
                    p.playing = false;
                    out.push_back({ MusicEvent::Stop });
                    return;
                }
                next = byte(1) == 0x33 ? e + 5 : e + 2;  // 0x33: another tempo (FUN_00499c1d, not done)
            }
            break;
        }
        p.position = next;
    }
}
