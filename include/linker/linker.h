#ifndef CRAFTLINKER_LINKER_H
#define CRAFTLINKER_LINKER_H

#include <filesystem>
#include <string>
#include <unordered_map>

#include <json.hpp>

namespace craftlinker
{
    extern int globalCounter;
    extern int functionCounter;
    extern std::unordered_map<std::string, int> functionMap;
    extern std::unordered_map<std::string, int> globalMap;
    extern std::unordered_map<std::string, bool> functionDefined;
    extern std::unordered_map<std::string, bool> globalDefined;
    extern std::string startContext;

    void replaceAll(std::string &str, const std::string &from, const std::string &to);
    void collectCategory(const char *label, const nlohmann::json &symbols,
                         std::unordered_map<std::string, int> &map,
                         std::unordered_map<std::string, bool> &defined,
                         int &counter);
    void init();
    void linkFile(const std::filesystem::path &file);
    void dealFile(const std::filesystem::path &file);
    void link();
} // namespace craftlinker

#endif // CRAFTLINKER_LINKER_H
