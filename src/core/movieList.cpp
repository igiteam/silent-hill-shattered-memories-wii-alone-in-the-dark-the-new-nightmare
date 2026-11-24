#include "movieList.h"

#include <cstdlib>
#include <filesystem>
#include <sstream>
#include <vector>

#include "dataFile.h"

std::map<int, std::string> loadMovieList(const std::string& dataFolder)
{
    std::map<int, std::string> movies;
    std::vector<uint8_t> data;
    if (!readDataFile((std::filesystem::path(dataFolder) / "MOVIES.TXT").string(), data))
        return movies;
    std::istringstream in(std::string(data.begin(), data.end()));
    std::string line;
    while (std::getline(in, line)) {
        const size_t comma = line.find(',');
        if (comma == std::string::npos)
            continue;
        const size_t second = line.find(',', comma + 1);
        std::string name = line.substr(comma + 1, second == std::string::npos ? std::string::npos : second - comma - 1);
        while (!name.empty() && (name.back() == ' ' || name.back() == '\r'))
            name.pop_back();
        if (!name.empty())
            movies[std::atoi(line.c_str())] = name;
    }
    return movies;
}
