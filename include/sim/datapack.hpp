#pragma once

#include <string>
#include <filesystem>
#include <map>
#include <sim/function.hpp>
namespace fs = std::filesystem;

namespace craftsim
{
    extern std::map<std::string, std::map<std::string, Function>> function_map; // key: namespace name, value: function name
    void loadDatapack(const fs::path &path);
} // namespace craftsim
