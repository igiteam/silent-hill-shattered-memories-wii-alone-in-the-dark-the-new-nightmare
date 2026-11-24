#include "scriptMachine.h"

#include <algorithm>
#include <cstring>

namespace {

constexpr uint32_t kScriptMark = 2;
constexpr size_t kHeaderSize = 0x0C;  // the code starts after it
constexpr size_t kTextSize = 0x80;     // a text on the stack: length byte and up to 127 letters
constexpr int kMaxSteps = 200000;      // per frame: a script that never stops is stopped

int textLength(const uint8_t text[kTextSize])
{
    return text[0] & 0x7F;
}

// Texts are equal as the game compares them (FUN_00411f5a): same length and
// the same first bytes, counting the length byte
bool textsEqual(const uint8_t a[kTextSize], const uint8_t b[kTextSize])
{
    return a[0] == b[0] && std::memcmp(a, b, size_t(textLength(a))) == 0;
}

// Adds b at the end of a, up to 127 letters (FUN_00411ee6)
void appendText(uint8_t a[kTextSize], const uint8_t b[kTextSize])
{
    const int start = textLength(a);
    int count = textLength(b);
    if (start + count > 0x7F)
        count = 0x7F - start;
    std::memcpy(a + 1 + start, b + 1, size_t(count));
    a[0] = uint8_t(start + count);
}

uint32_t readU32(const uint8_t* p)
{
    uint32_t value;
    std::memcpy(&value, p, 4);
    return value;
}

uint16_t readU16(const uint8_t* p)
{
    return uint16_t(p[0] | (p[1] << 8));
}

} // namespace

ScriptMachine::ScriptMachine(size_t stackSize, std::vector<uint8_t>* gameVariables)
    : stack_(stackSize, 0), variables_(gameVariables)
{
}

bool ScriptMachine::start(const uint8_t* block, size_t blockSize, uint32_t script,
                          const std::vector<int32_t>& parameters)
{
    block_ = nullptr;
    if (script == 0 || script + kHeaderSize > blockSize || readU32(block + script) != kScriptMark)
        return false;
    const uint16_t ownVariables = readU16(block + script + 4);
    if (readU16(block + script + 6) == 0 || ownVariables > stack_.size())
        return false;
    block_ = block;
    blockSize_ = blockSize;
    code_ = script + kHeaderSize;
    pc_ = 0;
    sp_ = bp_ = uint32_t(stack_.size());
    frameSize_ = ownVariables;
    retries_ = 0;
    waited_ = 0;
    parameters_.assign(parameters.begin(), parameters.begin() + std::ptrdiff_t(std::min<size_t>(parameters.size(), 16)));
    error_.clear();
    // The script's own variables are at the bottom of the stack, cleared
    std::memset(stack_.data(), 0, ownVariables);
    return true;
}

int32_t ScriptMachine::parameter(int number) const
{
    return number >= 0 && size_t(number) < parameters_.size() ? parameters_[size_t(number)] : 0;
}

void ScriptMachine::run(const ScriptNatives& natives, float milliseconds)
{
    if (retries_ > 0)
        waited_ += milliseconds;  // a call waiting since an earlier frame
    for (int steps = 0; busy() && steps < kMaxSteps; steps++)
        if (!step(natives))
            return;
    if (busy())
        fail("too many instructions in one frame");
}

std::string ScriptMachine::textArgument(int offset) const
{
    // Its first byte is its length (& 0x7F), then its letters
    std::string text;
    const size_t at = size_t(bp_) + size_t(offset);
    if (offset < 0 || at >= stack_.size())
        return text;
    const size_t length = std::min<size_t>(stack_[at] & 0x7F, kTextSize - 1);
    for (size_t k = 1; k <= length && at + k < stack_.size() && stack_[at + k] != 0; k++)
        text += char(stack_[at + k]);
    return text;
}

int32_t ScriptMachine::argument(int offset) const
{
    const size_t at = size_t(bp_) + size_t(offset);
    if (offset < 0 || at + 4 > stack_.size())
        return 0;
    int32_t value;
    std::memcpy(&value, &stack_[at], 4);
    return value;
}

void ScriptMachine::setArgument(int offset, int32_t value)
{
    const size_t at = size_t(bp_) + size_t(offset);
    if (offset >= 0 && at + 4 <= stack_.size())
        std::memcpy(&stack_[at], &value, 4);
}

void ScriptMachine::fail(const char* why)
{
    error_ = why;
    block_ = nullptr;
}

bool ScriptMachine::codeByte(uint8_t& out)
{
    if (code_ + pc_ + 1 > blockSize_) {
        fail("code beyond its block");
        return false;
    }
    out = block_[code_ + pc_];
    pc_ += 1;
    return true;
}

bool ScriptMachine::codeU16(uint16_t& out)
{
    if (code_ + pc_ + 2 > blockSize_) {
        fail("code beyond its block");
        return false;
    }
    out = readU16(block_ + code_ + pc_);
    pc_ += 2;
    return true;
}

bool ScriptMachine::codeI32(int32_t& out)
{
    if (code_ + pc_ + 4 > blockSize_) {
        fail("code beyond its block");
        return false;
    }
    std::memcpy(&out, block_ + code_ + pc_, 4);
    pc_ += 4;
    return true;
}

bool ScriptMachine::push(int32_t value)
{
    if (sp_ < 4) {
        fail("stack full");
        return false;
    }
    sp_ -= 4;
    std::memcpy(&stack_[sp_], &value, 4);
    return true;
}

bool ScriptMachine::pop(int32_t& value)
{
    if (size_t(sp_) + 4 > stack_.size()) {
        fail("stack empty");
        return false;
    }
    std::memcpy(&value, &stack_[sp_], 4);
    sp_ += 4;
    return true;
}

bool ScriptMachine::pushText(const uint8_t text[kTextSize])
{
    if (sp_ < kTextSize) {
        fail("stack full");
        return false;
    }
    sp_ -= uint32_t(kTextSize);
    std::memcpy(&stack_[sp_], text, size_t(textLength(text)) + 1);
    return true;
}

bool ScriptMachine::popText(uint8_t text[kTextSize])
{
    if (size_t(sp_) + kTextSize > stack_.size()) {
        fail("stack empty");
        return false;
    }
    std::memset(text, 0, kTextSize);
    std::memcpy(text, &stack_[sp_], size_t(stack_[sp_] & 0x7F) + 1);
    sp_ += uint32_t(kTextSize);
    return true;
}

bool ScriptMachine::readInt(const std::vector<uint8_t>& memory, size_t at, int32_t& out)
{
    if (at + 4 > memory.size()) {
        fail("variable out of its memory");
        return false;
    }
    std::memcpy(&out, &memory[at], 4);
    return true;
}

bool ScriptMachine::writeInt(std::vector<uint8_t>& memory, size_t at, int32_t value)
{
    if (at + 4 > memory.size()) {
        fail("variable out of its memory");
        return false;
    }
    std::memcpy(&memory[at], &value, 4);
    return true;
}

bool ScriptMachine::readText(const std::vector<uint8_t>& memory, size_t at, uint8_t text[kTextSize])
{
    if (at >= memory.size() || at + size_t(memory[at] & 0x7F) + 1 > memory.size()) {
        fail("text out of its memory");
        return false;
    }
    std::memset(text, 0, kTextSize);
    std::memcpy(text, &memory[at], size_t(memory[at] & 0x7F) + 1);
    return true;
}

bool ScriptMachine::writeText(std::vector<uint8_t>& memory, size_t at, const uint8_t text[kTextSize])
{
    const size_t count = size_t(textLength(text)) + 1;
    if (at + count > memory.size()) {
        fail("text out of its memory");
        return false;
    }
    std::memcpy(&memory[at], text, count);
    return true;
}

// One instruction (FUN_00411fc5). False when the machine stops for this
// frame (the script ended, waits, or failed)
bool ScriptMachine::step(const ScriptNatives& natives)
{
    std::vector<uint8_t>& variables = *variables_;
    const uint32_t instruction = pc_;
    uint8_t opcode;
    if (!codeByte(opcode))
        return false;
    uint16_t place = 0;
    int32_t a = 0, b = 0;
    uint8_t text[kTextSize] = {}, other[kTextSize] = {};
    // Two numbers off the stack: a was on top
    auto popTwo = [&]() { return pop(a) && pop(b); };

    switch (opcode) {
    case 0x00:
    case 0x2B:
        return true;
    case 0x01:
        return codeU16(place) && ((frameSize_ = place), true);
    case 0x02:
        return push(int32_t(bp_));
    case 0x03:
        return pop(a) && ((bp_ = uint32_t(a)), true);
    case 0x04:
        return codeI32(a) && push(a);
    case 0x05:  // a text of the script
        if (!codeU16(place))
            return false;
        if (code_ + place >= blockSize_ || code_ + place + size_t(block_[code_ + place] & 0x7F) + 1 > blockSize_) {
            fail("text beyond its block");
            return false;
        }
        std::memcpy(text, block_ + code_ + place, size_t(block_[code_ + place] & 0x7F) + 1);
        return pushText(text);
    case 0x06:  // number of the current call's frame
        return codeU16(place) && readInt(stack_, size_t(bp_) + place, a) && push(a);
    case 0x07:
        return codeU16(place) && readText(stack_, size_t(bp_) + place, text) && pushText(text);
    case 0x08:  // number of the script's own variables
        return codeU16(place) && readInt(stack_, place, a) && push(a);
    case 0x09:
        return codeU16(place) && readText(stack_, place, text) && pushText(text);
    case 0x0A:
        return codeU16(place) && pop(a) && writeInt(stack_, place, a);
    case 0x0B:
        return codeU16(place) && pop(a) && writeInt(stack_, size_t(bp_) + place, a);
    case 0x0C:
        return codeU16(place) && popText(text) && writeText(stack_, place, text);
    case 0x0D:
        return codeU16(place) && popText(text) && writeText(stack_, size_t(bp_) + place, text);
    case 0x0E:
        bp_ = sp_;
        return true;
    case 0x0F:  // drop bytes off the stack
        if (!codeU16(place))
            return false;
        if (size_t(sp_) + place > stack_.size()) {
            fail("stack empty");
            return false;
        }
        sp_ += place;
        return true;
    case 0x10:  // make room on the stack
        if (!codeU16(place))
            return false;
        if (sp_ < place) {
            fail("stack full");
            return false;
        }
        sp_ -= place;
        return true;
    case 0x11:  // call a part of the script
        if (!codeU16(place) || !push(int32_t(pc_)))
            return false;
        pc_ = place;
        return true;
    case 0x12: {  // call a function of the game; its frame is the top of the stack
        const uint32_t frame = bp_;
        bp_ = sp_;
        if (!codeU16(place))
            return false;
        NativeResult result = NativeResult::Continue;
        if (place < natives.count && natives.table[place])
            result = natives.table[place](*this, natives.context);
        else if (natives.missing)
            natives.missing(place, natives.context);
        switch (result) {
        case NativeResult::Continue:
            retries_ = 0;
            waited_ = 0;
            bp_ = frame;
            return true;
        case NativeResult::End:
            retries_ = 0;
            waited_ = 0;
            block_ = nullptr;
            return false;
        case NativeResult::Yield:
            retries_ = 0;
            waited_ = 0;
            return false;  // the game leaves the frame as it is here
        case NativeResult::Retry:
            pc_ = instruction;
            bp_ = frame;
            retries_++;
            return false;
        }
        return true;
    }
    case 0x13:  // back from a part of the script
        return pop(a) && ((pc_ = uint32_t(a)), true);
    case 0x14:
        block_ = nullptr;
        return false;
    case 0x15:
        if (!codeU16(place) || !pop(a))
            return false;
        if (a != 0)
            pc_ = place;
        return true;
    case 0x16:
        if (!codeU16(place) || !pop(a))
            return false;
        if (a == 0)
            pc_ = place;
        return true;
    case 0x17:
        return codeU16(place) && ((pc_ = place), true);
    case 0x18: return popTwo() && push(a == b);
    case 0x19: return popTwo() && push(a != b);
    case 0x1A: return popTwo() && push(a < b);
    case 0x1B: return popTwo() && push(b < a);
    case 0x1C: return popTwo() && push(a <= b);
    case 0x1D: return popTwo() && push(b <= a);
    case 0x1E: return popTwo() && push(a != 0 && b != 0);
    case 0x1F: return popTwo() && push(a != 0 || b != 0);
    case 0x20: return pop(a) && push(a == 0);
    case 0x21: return popTwo() && push(int32_t(uint32_t(a) * uint32_t(b)));
    case 0x22:
    case 0x23:
        if (!popTwo())
            return false;
        if (b == 0 || (a == INT32_MIN && b == -1)) {
            fail("division by zero");
            return false;
        }
        return push(opcode == 0x22 ? a / b : a % b);
    case 0x24: return popTwo() && push(int32_t(uint32_t(a) + uint32_t(b)));
    case 0x25:  // join two texts
        if (!popText(text) || !popText(other))
            return false;
        appendText(text, other);
        return pushText(text);
    case 0x26: return popTwo() && push(int32_t(uint32_t(a) - uint32_t(b)));
    case 0x27: return pop(a) && push(int32_t(0u - uint32_t(a)));
    case 0x28: return popTwo() && push((a != 0) == (b != 0));
    case 0x29: return popText(text) && popText(other) && push(textsEqual(text, other));
    case 0x2A:  // clear bytes at the top of the stack
        if (!codeU16(place))
            return false;
        if (size_t(sp_) + place > stack_.size()) {
            fail("stack empty");
            return false;
        }
        std::memset(&stack_[sp_], 0, place);
        return true;
    case 0x3B: return popTwo() && push((a != 0) != (b != 0));
    case 0x3C: return popText(text) && popText(other) && push(!textsEqual(text, other));
    case 0x3D:  // the game's variables
        return codeU16(place) && pop(a) && writeInt(variables, place, a);
    case 0x3E: {  // a bit of a game variable
        uint8_t bit;
        if (!codeU16(place) || !codeByte(bit) || !pop(b) || !readInt(variables, place, a))
            return false;
        const uint32_t mask = 1u << (bit & 0x1F);
        return writeInt(variables, place, int32_t(b != 0 ? uint32_t(a) | mask : uint32_t(a) & ~mask));
    }
    case 0x3F:
        return codeU16(place) && popText(text) && writeText(variables, place, text);
    case 0x40:
        return codeU16(place) && readInt(variables, place, a) && push(a);
    case 0x41: {
        uint8_t bit;
        return codeU16(place) && codeByte(bit) && readInt(variables, place, a)
               && push((uint32_t(a) >> (bit & 0x1F)) & 1);
    }
    case 0x42:
        return codeU16(place) && readText(variables, place, text) && pushText(text);
    case 0x43: return popTwo() && push(a & b);
    case 0x44: return popTwo() && push(a | b);
    case 0x45: return popTwo() && push(a ^ b);
    default:
        fail("unknown instruction");
        return false;
    }
}
