#ifndef CRAFTLANG_CONFIG_H
#define CRAFTLANG_CONFIG_H

#include <filesystem>
#include <string>

namespace craftlang {

struct Config
{
    std::string name = "..name..";
    std::string file_name;
    std::filesystem::path output_path;
};

extern Config config;

} // namespace craftlang

#endif // CRAFTLANG_CONFIG_H
