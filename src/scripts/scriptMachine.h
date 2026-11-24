#pragma once

// The game's script language: compiled scripts that a stack machine runs
// (FUN_00411fc5; docs/FORMATOS.md). The scripts live in the DRAM block of a
// level: a script is a place in that block, with the header u32 2, the size
// of its own variables (u16), the length of its code (u16), 4 more bytes and
// the code. Each instruction is a byte followed by its data.

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

class ScriptMachine;

// What the machine does after a function of the game (opcode 0x12)
enum class NativeResult {
    Continue = 0,  // go on with the next instruction
    End = 1,       // the script ends
    Yield = 2,     // stop until the next frame, then go on after the call
    Retry = 3,     // stop until the next frame, then call it again (waiting)
};

// A function of the game that scripts call by number; it reads its arguments
// with ScriptMachine::argument and gives results with setArgument
using ScriptNative = NativeResult (*)(ScriptMachine& machine, void* context);

// The functions a machine can call: table[number] (nullptr: not done), and
// what to tell when a script calls one that is not done
struct ScriptNatives {
    const ScriptNative* table = nullptr;
    size_t count = 0;
    void* context = nullptr;
    void (*missing)(int number, void* context) = nullptr;
};

class ScriptMachine {
public:
    // A machine with its own stack (the level's have 0xA00 bytes,
    // FUN_004667ac) and the game's variables, shared by all machines
    ScriptMachine(size_t stackSize, std::vector<uint8_t>* gameVariables);

    // True while it runs a script
    bool busy() const { return block_ != nullptr; }

    // Starts the script at a place of a block, with its parameters (up to 16;
    // FUN_00411918). False (and the machine stays free) if there is no script
    // there
    bool start(const uint8_t* block, size_t blockSize, uint32_t script, const std::vector<int32_t>& parameters = {});

    // Runs the script until it ends or waits for the next frame (FUN_00411a32);
    // milliseconds since the frame before (for waitedMilliseconds)
    void run(const ScriptNatives& natives, float milliseconds);

    // A parameter the script was started with (FUN_00411ae1; 0 for none)
    int32_t parameter(int number) const;

    // For a function of the game: its argument at that many bytes from the
    // top of the stack (FUN_00411af8), and writing one (FUN_00411b6f)
    int32_t argument(int offset) const;
    void setArgument(int offset, int32_t value);
    // A text argument (128 bytes on the stack, FUN_00411b17) at that offset:
    // its first byte is its length, then its letters
    std::string textArgument(int offset) const;

    // How many frames the current call has been retried (FUN_00411bf5), and
    // for how long, in milliseconds
    int retries() const { return retries_; }
    float waitedMilliseconds() const { return waited_; }

    // Why the last script stopped by itself (bad data), or empty
    const std::string& error() const { return error_; }

private:
    bool step(const ScriptNatives& natives);
    void fail(const char* why);

    // Reading the code of the script
    bool codeByte(uint8_t& out);
    bool codeU16(uint16_t& out);
    bool codeI32(int32_t& out);

    // The stack (grows down) and the memories, with bounds checked
    bool push(int32_t value);
    bool pop(int32_t& value);
    bool pushText(const uint8_t text[128]);
    bool popText(uint8_t text[128]);
    bool readInt(const std::vector<uint8_t>& memory, size_t at, int32_t& out);
    bool writeInt(std::vector<uint8_t>& memory, size_t at, int32_t value);
    bool readText(const std::vector<uint8_t>& memory, size_t at, uint8_t text[128]);
    bool writeText(std::vector<uint8_t>& memory, size_t at, const uint8_t text[128]);

    std::vector<uint8_t> stack_;
    std::vector<uint8_t>* variables_;
    const uint8_t* block_ = nullptr;
    size_t blockSize_ = 0;
    size_t code_ = 0;        // where the code starts in the block
    uint32_t pc_ = 0;        // the next instruction, from the start of the code
    uint32_t sp_ = 0;        // top of the stack (bytes from its start)
    uint32_t bp_ = 0;        // frame of the current call
    uint32_t frameSize_ = 0; // opcode 0x01
    int retries_ = 0;
    float waited_ = 0;
    std::vector<int32_t> parameters_;
    std::string error_;
};
