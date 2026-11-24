#include "window.h"

#include <SDL3/SDL.h>

namespace {

SDL_Window* s_window = nullptr;

// DirectInput codes (the PC keyboard's scan codes, 0xE0 keys as 0x80 + code)
// and the keys of SDL
struct KeyCode {
    int directInput;
    SDL_Scancode sdl;
};
const KeyCode kKeyCodes[] = {
    { 0x01, SDL_SCANCODE_ESCAPE }, { 0x02, SDL_SCANCODE_1 }, { 0x03, SDL_SCANCODE_2 }, { 0x04, SDL_SCANCODE_3 },
    { 0x05, SDL_SCANCODE_4 }, { 0x06, SDL_SCANCODE_5 }, { 0x07, SDL_SCANCODE_6 }, { 0x08, SDL_SCANCODE_7 },
    { 0x09, SDL_SCANCODE_8 }, { 0x0A, SDL_SCANCODE_9 }, { 0x0B, SDL_SCANCODE_0 }, { 0x0C, SDL_SCANCODE_MINUS },
    { 0x0D, SDL_SCANCODE_EQUALS }, { 0x0E, SDL_SCANCODE_BACKSPACE }, { 0x0F, SDL_SCANCODE_TAB },
    { 0x10, SDL_SCANCODE_Q }, { 0x11, SDL_SCANCODE_W }, { 0x12, SDL_SCANCODE_E }, { 0x13, SDL_SCANCODE_R },
    { 0x14, SDL_SCANCODE_T }, { 0x15, SDL_SCANCODE_Y }, { 0x16, SDL_SCANCODE_U }, { 0x17, SDL_SCANCODE_I },
    { 0x18, SDL_SCANCODE_O }, { 0x19, SDL_SCANCODE_P }, { 0x1A, SDL_SCANCODE_LEFTBRACKET },
    { 0x1B, SDL_SCANCODE_RIGHTBRACKET }, { 0x1C, SDL_SCANCODE_RETURN }, { 0x1D, SDL_SCANCODE_LCTRL },
    { 0x1E, SDL_SCANCODE_A }, { 0x1F, SDL_SCANCODE_S }, { 0x20, SDL_SCANCODE_D }, { 0x21, SDL_SCANCODE_F },
    { 0x22, SDL_SCANCODE_G }, { 0x23, SDL_SCANCODE_H }, { 0x24, SDL_SCANCODE_J }, { 0x25, SDL_SCANCODE_K },
    { 0x26, SDL_SCANCODE_L }, { 0x27, SDL_SCANCODE_SEMICOLON }, { 0x28, SDL_SCANCODE_APOSTROPHE },
    { 0x29, SDL_SCANCODE_GRAVE }, { 0x2A, SDL_SCANCODE_LSHIFT }, { 0x2B, SDL_SCANCODE_BACKSLASH },
    { 0x2C, SDL_SCANCODE_Z }, { 0x2D, SDL_SCANCODE_X }, { 0x2E, SDL_SCANCODE_C }, { 0x2F, SDL_SCANCODE_V },
    { 0x30, SDL_SCANCODE_B }, { 0x31, SDL_SCANCODE_N }, { 0x32, SDL_SCANCODE_M }, { 0x33, SDL_SCANCODE_COMMA },
    { 0x34, SDL_SCANCODE_PERIOD }, { 0x35, SDL_SCANCODE_SLASH }, { 0x36, SDL_SCANCODE_RSHIFT },
    { 0x37, SDL_SCANCODE_KP_MULTIPLY }, { 0x38, SDL_SCANCODE_LALT }, { 0x39, SDL_SCANCODE_SPACE },
    { 0x3A, SDL_SCANCODE_CAPSLOCK }, { 0x3B, SDL_SCANCODE_F1 }, { 0x3C, SDL_SCANCODE_F2 }, { 0x3D, SDL_SCANCODE_F3 },
    { 0x3E, SDL_SCANCODE_F4 }, { 0x3F, SDL_SCANCODE_F5 }, { 0x40, SDL_SCANCODE_F6 }, { 0x41, SDL_SCANCODE_F7 },
    { 0x42, SDL_SCANCODE_F8 }, { 0x43, SDL_SCANCODE_F9 }, { 0x44, SDL_SCANCODE_F10 },
    { 0x47, SDL_SCANCODE_KP_7 }, { 0x48, SDL_SCANCODE_KP_8 }, { 0x49, SDL_SCANCODE_KP_9 },
    { 0x4A, SDL_SCANCODE_KP_MINUS }, { 0x4B, SDL_SCANCODE_KP_4 }, { 0x4C, SDL_SCANCODE_KP_5 },
    { 0x4D, SDL_SCANCODE_KP_6 }, { 0x4E, SDL_SCANCODE_KP_PLUS }, { 0x4F, SDL_SCANCODE_KP_1 },
    { 0x50, SDL_SCANCODE_KP_2 }, { 0x51, SDL_SCANCODE_KP_3 }, { 0x52, SDL_SCANCODE_KP_0 },
    { 0x53, SDL_SCANCODE_KP_PERIOD }, { 0x57, SDL_SCANCODE_F11 }, { 0x58, SDL_SCANCODE_F12 },
    { 0x9C, SDL_SCANCODE_KP_ENTER }, { 0x9D, SDL_SCANCODE_RCTRL }, { 0xB5, SDL_SCANCODE_KP_DIVIDE },
    { 0xB8, SDL_SCANCODE_RALT }, { 0xC7, SDL_SCANCODE_HOME }, { 0xC8, SDL_SCANCODE_UP },
    { 0xC9, SDL_SCANCODE_PAGEUP }, { 0xCB, SDL_SCANCODE_LEFT }, { 0xCD, SDL_SCANCODE_RIGHT },
    { 0xCF, SDL_SCANCODE_END }, { 0xD0, SDL_SCANCODE_DOWN }, { 0xD1, SDL_SCANCODE_PAGEDOWN },
    { 0xD2, SDL_SCANCODE_INSERT }, { 0xD3, SDL_SCANCODE_DELETE },
};

int directInputCode(SDL_Scancode sdl)
{
    for (const KeyCode& code : kKeyCodes)
        if (code.sdl == sdl)
            return code.directInput;
    return -1;
}

} // namespace

bool openWindow(const std::string& title, int width, int height)
{
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
        return false;
    s_window = SDL_CreateWindow(title.c_str(), width, height, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    return s_window != nullptr;
}

void closeWindow()
{
    if (s_window)
        SDL_DestroyWindow(s_window);
    s_window = nullptr;
    SDL_Quit();
}

bool pollWindowEvents(std::vector<KeyPress>& keys, std::vector<int>& codes)
{
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
            return false;
        if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.button == SDL_BUTTON_MIDDLE)
            keys.push_back(KeyPress::PlaceCharacter);
        if (event.type != SDL_EVENT_KEY_DOWN)
            continue;
        if (!event.key.repeat) {
            const int code = directInputCode(event.key.scancode);
            if (code >= 0)
                codes.push_back(code);
            if (event.key.key == SDLK_RETURN || event.key.key == SDLK_KP_ENTER)
                keys.push_back(KeyPress::Confirm);
        }
        switch (event.key.key) {
        case SDLK_PAGEUP: keys.push_back(KeyPress::PreviousCamera); break;
        case SDLK_PAGEDOWN: keys.push_back(KeyPress::NextCamera); break;
        case SDLK_HOME: keys.push_back(KeyPress::PreviousLevel); break;
        case SDLK_END: keys.push_back(KeyPress::NextLevel); break;
        case SDLK_F3: keys.push_back(KeyPress::NextLight); break;
        case SDLK_C: keys.push_back(KeyPress::NextCharacter); break;
        case SDLK_F: keys.push_back(KeyPress::ToggleForeground); break;
        case SDLK_J: keys.push_back(KeyPress::ToggleActors); break;
        case SDLK_P: keys.push_back(KeyPress::NextSound); break;
        case SDLK_N: keys.push_back(KeyPress::NextAnimation); break;
        case SDLK_B: keys.push_back(KeyPress::PreviousAnimation); break;
        case SDLK_H: keys.push_back(KeyPress::NextLeftHand); break;
        case SDLK_G: keys.push_back(KeyPress::NextRightHand); break;
        case SDLK_K: keys.push_back(KeyPress::NextAction); break;
        case SDLK_F2: keys.push_back(KeyPress::NextRenderer); break;
        case SDLK_F5: keys.push_back(KeyPress::QuickSave); break;
        case SDLK_F8: keys.push_back(KeyPress::QuickLoad); break;
        case SDLK_ESCAPE: keys.push_back(KeyPress::Quit); break;
        default: break;
        }
    }
    return true;
}

bool keyHeld(int code)
{
    const bool* state = SDL_GetKeyboardState(nullptr);
    for (const KeyCode& key : kKeyCodes)
        if (key.directInput == code)
            return state[key.sdl];
    return false;
}

void windowPixelSize(int& width, int& height)
{
    SDL_GetWindowSizeInPixels(s_window, &width, &height);
}

uint64_t milliseconds()
{
    return SDL_GetTicks();
}

void mouseDesktopPosition(float& x, float& y)
{
    x = y = 0;
    SDL_GetGlobalMouseState(&x, &y);
}

bool mouseButtonHeld(bool right)
{
    const SDL_MouseButtonFlags held = SDL_GetMouseState(nullptr, nullptr);
    return (held & (right ? SDL_BUTTON_RMASK : SDL_BUTTON_LMASK)) != 0;
}

void mousePixelPosition(float& x, float& y)
{
    // SDL gives it in window units; with a high pixel density they are not pixels
    float mouseX = 0, mouseY = 0;
    SDL_GetMouseState(&mouseX, &mouseY);
    int units = 0, unitsHigh = 0, pixels = 0, pixelsHigh = 0;
    SDL_GetWindowSize(s_window, &units, &unitsHigh);
    SDL_GetWindowSizeInPixels(s_window, &pixels, &pixelsHigh);
    x = units > 0 ? mouseX * float(pixels) / float(units) : mouseX;
    y = unitsHigh > 0 ? mouseY * float(pixelsHigh) / float(unitsHigh) : mouseY;
}

void setWindowTitle(const std::string& title)
{
    SDL_SetWindowTitle(s_window, title.c_str());
}

void* nativeWindowHandle()
{
#if defined(_WIN32)
    return SDL_GetPointerProperty(SDL_GetWindowProperties(s_window), SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
#else
    return nullptr; // other systems: later
#endif
}

std::string lastWindowError()
{
    return SDL_GetError();
}

void showErrorMessage(const std::string& title, const std::string& message)
{
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, title.c_str(), message.c_str(), s_window);
}
