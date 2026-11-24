#pragma once

// The program's window (SDL3) and the keys it receives. The keys of the game
// itself are given by their DirectInput codes (the PC keyboard's scan codes,
// as the game's keys.bin keeps them); the viewer's own ones as KeyPress.

#include <cstdint>
#include <string>
#include <vector>

enum class KeyPress {
    PreviousCamera, // Page Up
    NextCamera,     // Page Down
    PreviousLevel,  // Home
    NextLevel,      // End
    PlaceCharacter, // middle mouse button (the left and right ones are the game's)
    Confirm,        // Enter
    NextLight,      // F3: the background with the flashlight, all lit, all dark
    NextCharacter,  // C: the character shown (Carnby, Aline, none)
    NextAnimation,  // N
    PreviousAnimation, // B
    NextLeftHand,   // H
    NextRightHand,  // G
    NextAction,     // K
    ToggleForeground, // F: highlight the foreground
    NextSound,        // P: play the next sound of the level's bank
    ToggleActors,     // J: every actor of the level appears (again: they go away)
    QuickSave,        // F5: save the game (as the game's quick save)
    QuickLoad,        // F8: load it
    NextRenderer,   // F2
    Quit,           // Esc
};

// Opens the window, resizable, with that size in pixels. False if SDL fails
// (the reason is in lastWindowError)
bool openWindow(const std::string& title, int width, int height);

// Closes the window and SDL
void closeWindow();

// Reads the pending events: the viewer's keys pressed go to keys, and the
// DirectInput codes of all the keys pressed (not repeats) to codes. False
// once the window has been closed
bool pollWindowEvents(std::vector<KeyPress>& keys, std::vector<int>& codes);

// Whether the key with that DirectInput code is held now (false for a code
// the window does not know)
bool keyHeld(int directInputCode);

// Size of the inside of the window, in pixels
void windowPixelSize(int& width, int& height);

// Milliseconds since the program started
uint64_t milliseconds();

// Where the mouse is on the whole desktop (pixels): known even before the
// window has had any event of the mouse
void mouseDesktopPosition(float& x, float& y);

// Whether the left (or right) mouse button is held now
bool mouseButtonHeld(bool right);

// Where the mouse is over the window, in pixels from the top left corner
void mousePixelPosition(float& x, float& y);

// Changes the title of the window
void setWindowTitle(const std::string& title);

// Native handle of the window (HWND on Windows), for the renderer
void* nativeWindowHandle();

// Last error of SDL
std::string lastWindowError();

// Shows an error in a message box over the window
void showErrorMessage(const std::string& title, const std::string& message);
