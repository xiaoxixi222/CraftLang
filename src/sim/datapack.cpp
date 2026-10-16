#include <sim/datapack.hpp>
#include <fsUtils.hpp>
#include <sim/sim.hpp>
#include <iostream>
#include <string>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <json.hpp>

namespace fs = std::filesystem;
using json = nlohmann::json;
namespace craftsim
{
    std::map<std::string, std::map<std::string, Function>> function_map;
    void loadDatapack(const fs::path &path)
    {
        json pack_meta = json::parse(fs_utils::readFile(path / "pack.mcmeta"));
        auto data_files = fs_utils::listDirectory(path / "data");
        for(auto &file : data_files)
        {
#ifdef _DEBUG
            std::cout << "Loading file: " << file << std::endl;
#endif
            auto namespace_name = fs_utils::stem(file);
            if(fs_utils::exists(file / "function")){
                auto functions = fs_utils::listDirectory(file / "function");
                for(auto &func : functions)
                {
                    auto func_name = fs_utils::stem(func);
                    auto func_content = fs_utils::readFile(func);
                    function_map[namespace_name][func_name] = {func_name, func_content};
                }
            }
        }
    }
} // namespace craftsim
