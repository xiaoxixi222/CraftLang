#include <linker/config.h>

#include <json.hpp>

#include <iostream>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <string>
#include <vector>
#include <algorithm>

namespace fs = std::filesystem;
using json = nlohmann::json;

extern craftlinker::Config config;
namespace craftlinker
{
    int globalCounter = 0, functionCounter = 0;
    std::unordered_map<std::string, int> functionMap, globalMap;
    std::unordered_map<std::string, bool> functionDefined, globalDefined;
    void collectCategory(const char *label, const json &symbols, std::unordered_map<std::string, int> &map, std::unordered_map<std::string, bool> &defined, int &counter)
    {
        for (const auto &entry : symbols)
        {
            std::string name = entry["name"];
            bool isExtern = entry["isExtern"];
            auto it = map.find(name);
            if (it == map.end())
            {
                map[name] = counter++;
                defined[name] = !isExtern;
#ifdef _DEBUG
                std::cout << "[debug] " << label << " \"" << name << "\" (isExtern=" << (isExtern ? "true" : "false") << ") -> new number " << map[name] << std::endl;
#endif
            }
            else if (!defined[name] && !isExtern)
            {
                defined[name] = true;
#ifdef _DEBUG
                std::cout << "[debug] " << label << " \"" << name << "\" definition found, shares number " << map[name] << std::endl;
#endif
            }
            else if (defined[name] && !isExtern)
            {
                throw std::runtime_error(std::string("duplicate definition: ") + name);
            }
#ifdef _DEBUG
            else
            {
                std::cout << "[debug] " << label << " \"" << name << "\" (isExtern=" << (isExtern ? "true" : "false") << ") shares number " << map[name] << std::endl;
            }
#endif
        }
    }
    void init()
    {
        fs::path pack_meta = config.outputPath / config.name / "pack.mcmeta";
        std::ofstream pack_meta_file(pack_meta, std::ios::out | std::ios::trunc);
        if (!pack_meta_file)
        {
            std::cerr << "Failed to create pack.mcmeta file." << std::endl;
            throw std::runtime_error("Failed to create pack.mcmeta file.");
            return;
        }
        pack_meta_file << "{\"pack\":{\"pack_format\":" << config.pack_format << ",\"description\":\"" << config.description << "\"}}";
        pack_meta_file.close();
        fs::create_directories(config.outputPath / config.name / "data" / config.name / "function");
    }
    void linkFile(const fs::path &file)
    {
        std::ifstream file_stream(file);
        if (!file_stream)
        {
            std::cerr << "Failed to open file: " << file << std::endl;
            throw std::runtime_error("Failed to open file.");
            return;
        }
        std::stringstream buffer;
        buffer << file_stream.rdbuf();
        std::string content = buffer.str();
        json symbols = json::parse(content);
        json functions = symbols["function"];
        json globals = symbols["Global"];
        collectCategory("function", functions, functionMap, functionDefined, functionCounter);
        collectCategory("global", globals, globalMap, globalDefined, globalCounter);
    }
    void link()
    {
        fs::remove_all(config.outputPath / config.name);
        fs::create_directories(config.outputPath / config.name);
        init();
        for (const auto &file : config.linkFile)
        {
            linkFile(file); // 调用 linkFile 函数处理每个链接文件
        }
        for (const auto &pair : functionDefined)
        {
            if (!pair.second)
                throw std::runtime_error(std::string("undefined extern function: ") + pair.first);
        }
        for (const auto &pair : globalDefined)
        {
            if (!pair.second)
                throw std::runtime_error(std::string("undefined extern global: ") + pair.first);
        }
#ifdef _DEBUG
        std::vector<std::pair<std::string, int>> functionList(functionMap.begin(), functionMap.end());
        std::vector<std::pair<std::string, int>> globalList(globalMap.begin(), globalMap.end());
        std::sort(functionList.begin(), functionList.end());
        std::sort(globalList.begin(), globalList.end());
        std::cout << "[debug] === final function list ===" << std::endl;
        for (const auto &pair : functionList)
        {
            std::cout << "[debug] function " << pair.first << " -> " << pair.second << std::endl;
        }
        std::cout << "[debug] === final global list ===" << std::endl;
        for (const auto &pair : globalList)
        {
            std::cout << "[debug] global " << pair.first << " -> " << pair.second << std::endl;
        }
#endif
    }
} // namespace craftlinker