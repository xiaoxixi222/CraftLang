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
    std::string startContext = "";
    void replaceAll(std::string &str, const std::string &from, const std::string &to)
    {
        if (from.empty())
            return; // 防止死循环

        size_t start_pos = 0;
        while ((start_pos = str.find(from, start_pos)) != std::string::npos)
        {
            str.replace(start_pos, from.length(), to);
            start_pos += to.length(); // 移动到替换后的末尾，继续往后找
        }
    }
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
    void dealFile(const fs::path &file)
    {
        std::unordered_map<int, int> functionDealMap, globalDealMap;
        std::string name = file.stem().string();
        fs::path input_path = file.parent_path() / (name.substr(0, name.size() - 8) + "_o");
        fs::path output_path = config.outputPath / config.name / "data" / config.name;
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
        for (const auto &function : functions)
        {
            std::string function_name = function["name"];
            int from = function["number"];
            int goal = functionMap[function_name];
            functionDealMap[from] = goal;
        }
        for (const auto &global : globals)
        {
            std::string global_name = global["name"];
            int from = global["number"];
            int goal = globalMap[global_name];
            globalDealMap[from] = goal;
        }
        for (const auto &entry : fs::directory_iterator(input_path / "function"))
        {
            std::string functionName = entry.path().stem().string();
            std::string outputName = functionName;
            std::ifstream functionStream(entry.path());
            if (!functionStream)
            {
                std::cerr << "Failed to open file: " << entry.path() << std::endl;
                throw std::runtime_error("Failed to open file.");
                return;
            }
            std::stringstream functionBuffer;
            functionBuffer << functionStream.rdbuf();
            std::string functionContent = functionBuffer.str();
            for (const auto &pair : functionDealMap)
            {
                replaceAll(functionContent, std::string(".f.") + std::to_string(pair.first) + ".f.", std::to_string(pair.second));
                replaceAll(outputName, std::string(".f.") + std::to_string(pair.first) + ".f.", std::to_string(pair.second));
            }
            for (const auto &pair : globalDealMap)
                replaceAll(functionContent, std::string(".g.") + std::to_string(pair.first) + ".g.", std::to_string(pair.second));
            replaceAll(functionContent, "..name..", config.name);
            if (functionName == "start")
            {
                startContext += functionContent + "\n";
            }
            else
            {
                fs::path function_output_file = output_path / "function" / (outputName + ".mcfunction");
                std::ofstream function_output_file_stream(function_output_file, std::ios::out | std::ios::trunc);
                if (!function_output_file_stream)
                {
                    std::cerr << "Failed to create function output file: " << function_output_file << std::endl;
                    throw std::runtime_error("Failed to create function output file.");
                }
                function_output_file_stream << functionContent;
                function_output_file_stream.close();
            }
        }
        if (fs::exists(input_path / "other"))
        { // 处理其他文件
            for (const auto &entry : fs::recursive_directory_iterator(input_path / "other"))
            {
                if (!entry.is_regular_file())
                    continue; // 跳过目录

                std::string otherName = entry.path().stem().string();
                std::ifstream otherStream(entry.path());
                if (!otherStream)
                {
                    std::cerr << "Failed to open file: " << entry.path() << std::endl;
                    throw std::runtime_error("Failed to open file.");
                    return;
                }
                std::stringstream otherBuffer;
                otherBuffer << otherStream.rdbuf();
                std::string otherContent = otherBuffer.str();
                for (const auto &pair : functionDealMap)
                    replaceAll(otherContent, std::string(".f.") + std::to_string(pair.first) + ".f.", std::to_string(pair.second));
                for (const auto &pair : globalDealMap)
                    replaceAll(otherContent, std::string(".g.") + std::to_string(pair.first) + ".g.", std::to_string(pair.second));
                replaceAll(otherContent, "..name..", config.name);

                fs::path rel = fs::relative(entry.path(), input_path / "other");
                fs::path output_path_file = output_path.parent_path() / rel;
#ifdef _DEBUG
                std::cout << "[debug] entry: " << entry.path() << std::endl;
                std::cout << "[debug] other: " << input_path / "other" << std::endl;
                std::cout << "[debug] rel: " << rel << std::endl;
                std::cout << "[debug] output_path_file: " << output_path_file << std::endl;
#endif
                if (!fs::exists(output_path_file.parent_path()))
                {
                    fs::create_directories(output_path_file.parent_path());
                }
                std::ofstream other_output_file_stream(output_path_file, std::ios::out | std::ios::trunc);
                if (!other_output_file_stream)
                {
                    std::cerr << "Failed to create other output file: " << output_path_file << std::endl;
                    throw std::runtime_error("Failed to create other output file.");
                }
                other_output_file_stream << otherContent;
                other_output_file_stream.close();
            }
        }

#ifdef _DEBUG
        std::cout << "[debug] === deal file ===" << std::endl;
        std::cout << "[debug] input_path: " << input_path << std::endl;
        std::cout << "[debug] output_path: " << output_path << std::endl;
#endif
        if (!fs::exists(input_path))
        {
            std::cerr << "Input file does not exist: " << input_path << std::endl;
            throw std::runtime_error("Input file does not exist.");
        }
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

        startContext = std::string("scoreboard objectives remove functionSpace\nscoreboard objectives add functionSpace dummy\nscoreboard players set ") + config.name + " functionSpace 1\n";
        for (const auto &file : config.linkFile)
        {
            dealFile(file); // 调用 dealFile 函数处理每个链接文件
        }
        std::ofstream start_context_file(config.outputPath / config.name / "data" / config.name / "function" / "start.mcfunction", std::ios::out | std::ios::trunc);
        if (!start_context_file)
        {
            std::cerr << "Failed to create start context file: " << config.outputPath / config.name / "data" / config.name / "function" / "start.mcfunction" << std::endl;
            throw std::runtime_error("Failed to create start context file.");
        }
        if (functionMap.find("main") != functionMap.end())
        {
            startContext +=
                std::string("execute store result storage ") + config.name + " functionSpace int 1 run scoreboard players get " + config.name + " functionSpace\n" +
                "function " + config.name + ":" + std::to_string(functionMap["main"]) + " with storage " +
                config.name;
        }
        start_context_file << startContext;
        start_context_file.close();
    }
} // namespace craftlinker