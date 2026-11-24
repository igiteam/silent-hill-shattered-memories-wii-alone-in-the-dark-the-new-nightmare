#include "userData.h"

#include <SDL3/SDL.h>

#include <filesystem>
#include <fstream>
#include <iterator>

std::string userDataFolder()
{
    const char* base = SDL_GetBasePath();
    if (!base)
        return {};
    const std::filesystem::path folder = std::filesystem::path(base) / "UserData";
    std::error_code error;
    std::filesystem::create_directories(folder, error);
    if (error)
        return {};
    return folder.string() + "/";
}

std::string readUserText(const std::string& name)
{
    const std::string folder = userDataFolder();
    if (folder.empty())
        return {};
    std::ifstream in(folder + name, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

bool writeUserText(const std::string& name, const std::string& text)
{
    const std::string folder = userDataFolder();
    if (folder.empty())
        return false;
    std::ofstream out(folder + name, std::ios::binary | std::ios::trunc);
    out << text;
    return bool(out);
}
