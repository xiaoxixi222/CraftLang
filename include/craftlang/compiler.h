#ifndef CRAFTLANG_COMPILER_H
#define CRAFTLANG_COMPILER_H

#include <clang-c/Index.h>

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace craftlang
{
    struct Parm
    {
        CXType kind;
        std::string name;
        int number;
    };

    struct Function
    {
        std::string name;
        CXType return_type;
        std::vector<Parm> parms;
        int number;
        CXCursor cursor;
    };

    struct Var
    {
        std::string name;
        CXType type;
        int number;
    };

    extern const std::vector<std::string> intrinsicFunctions;

    extern std::unordered_map<std::string, Function> functionMap;
    extern int functionCounter;

    extern std::string current_content;
    extern std::string startFuncitonContent;
    extern std::filesystem::path current_file;
    extern std::filesystem::path function_path;
    extern Function current_function;

    extern std::unordered_map<std::string, Var> localVarsToInt;
    extern std::unordered_map<std::string, Var> globalVarsToInt;
    extern int localVarCounter;
    extern int globalVarCounter;

    std::string addDollarPrefix(const std::string &text);
    std::string initVar(std::string name, CXTypeKind type, std::string value);
    void deal_cursor(CXCursor cursor);
    void compile(CXCursor root_cursor);

} // namespace craftlang

#endif // CRAFTLANG_COMPILER_H
