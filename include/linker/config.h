#ifndef CRAFTLINKER_CONFIG_H
#define CRAFTLINKER_CONFIG_H

#include <filesystem>
#include <string>
#include <vector>

namespace craftlinker
{
    struct Config
    {
        std::string name;
        int pack_format;
        std::string description;
        std::vector<std::filesystem::path> linkFile;
        std::filesystem::path outputPath;
    };
    extern Config config;
} // namespace craftlinker

#endif // CRAFTLINKER_CONFIG_H
