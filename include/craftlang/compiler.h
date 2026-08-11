#ifndef CRAFTLANG_COMPILER_H
#define CRAFTLANG_COMPILER_H

#include <clang-c/Index.h>
#include <json.hpp>

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
        bool isExtern;
    };

    struct Var
    {
        std::string name;
        std::string objective;
        CXType type;
        int number;
        bool isExtern;
    };

    struct ExprResult
    {
        int tmp_number;
    };

    struct LValue{
        std::string objective;
    };
    extern const std::vector<CXBinaryOperatorKind> binaryOperatorNeedLValue;

    extern std::unordered_map<std::string, Function> functionMap;
    extern int functionCounter;

    extern std::string current_content;
    extern std::string startFunctionContent;
    extern std::filesystem::path current_file;
    extern std::filesystem::path function_path;
    extern Function current_function;

    extern std::unordered_map<std::string, Var> localVarsToInt;
    extern std::unordered_map<std::string, Var> globalVarsToInt;
    extern int localVarCounter;
    extern int globalVarCounter;

    extern nlohmann::json symbols;
    extern int tmp_counter;
    extern const std::vector<CXCursorKind> exprStatementKinds;

    std::string addDollarPrefix(const std::string &text);
    std::string initVar(std::string name, CXTypeKind type);
    std::string setVarToVar(std::string name, std::string value, CXTypeKind type);
    std::string setConstToVar(std::string name, std::string value, CXTypeKind type);
    ExprResult start_deal_expr(CXCursor expr);
    ExprResult deal_expr(CXCursor expr);
    LValue deal_lvalue(CXCursor lvalue);
    void deal_cursor(CXCursor cursor);
    void compile(CXCursor root_cursor);

} // namespace craftlang

#endif // CRAFTLANG_COMPILER_H
