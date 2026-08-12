#include <json.hpp>

#include <iostream>
#include <string>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>


#include <linker/config.h>
#include <linker/linker.h>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace craftlinker
{
    Config config;
}
int main(int argc, char *argv[])
{
    using craftlinker::config;
    std::cout << "craftlinker: CraftLang linker (under construction)\n";
    if (argc != 2)
    {
        throw std::runtime_error("craftlinker: Incorrect number of arguments. Usage: craftlinker <setting.json>");
    }
    std::string settingsFile = argv[1];
    std::cout << "craftlinker: Loading settings from " << settingsFile << "\n";

    // 1. 读文件内容
    std::ifstream file(settingsFile);
    if (!file.is_open())
    {
        std::cerr << "craftlinker: failed to open " << settingsFile << "\n";
        return 1;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();

    json settings = json::parse(content);
    config.name = settings["name"];
    config.pack_format = settings["pack_format"];
    config.description = settings["description"];
    for (const auto &f : settings["linkFile"])
        config.linkFile.push_back(fs::u8path(f.get<std::string>()));
    config.outputPath = fs::u8path(settings["outputPath"].get<std::string>());
    std::cout << "craftlinker: Settings loaded\n";
    std::cout << "craftlinker: Name: " << config.name << "\n";
    std::cout << "craftlinker: Pack Format: " << config.pack_format << "\n";
    std::cout << "craftlinker: Description: " << config.description << "\n";
    std::cout << "craftlinker: Link Files: ";
    for (auto &file : config.linkFile)
    {
        std::cout << file << " ";
    }
    std::cout << "\n";
    std::cout << "craftlinker: Output Path: " << config.outputPath << "\n";
    std::cout << "craftlinker: Linking...\n";
    craftlinker::link();
    std::cout << "craftlinker: Linking complete\n";
    return 0;
}
