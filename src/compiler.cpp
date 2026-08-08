#include <craftlang/compiler.h>
#include <craftlang/lang.h>
#include <craftlang/config.h>

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;
extern craftlang::Config config; // 使用 craftlang 的配置结构体
namespace craftlang
{
    extern const std::vector<std::string> intrinsicFunctions = {
        "print_int",
    };
    std::unordered_map<std::string, Function> functionMap = {};
    int functionCounter = 0;

    std::string current_content = "", startFuncitonContent = "";
    fs::path current_file = "";
    fs::path function_path = "";
    Function current_function;
    std::unordered_map<std::string, Var> localVarsToInt = {};  // 局部变量的编号
    std::unordered_map<std::string, Var> globalVarsToInt = {}; // 全局变量的编号
    int localVarCounter = 0, globalVarCounter = 0;

    int tmp_counter = 0;
    extern const std::vector<CXCursorKind> exprStatementKinds = {
        CXCursor_CallExpr,       // print_int(c); add(a,b);
        CXCursor_BinaryOperator, // d = c;
    };

    std::string addDollarPrefix(const std::string &text)
    {
        std::string result;
        std::istringstream iss(text); // 已经 include 了 <sstream>
        std::string line;
        bool first = true;
        while (std::getline(iss, line))
        {
            if (!first)
                result += "\n";
            first = false;
            if (line.find("$") != std::string::npos)
                result += "$";
            result += line;
        }
        return result;
    }

    std::string initVar(std::string name, CXTypeKind type)
    {
        std::string init = "";
        switch (type)
        {
        case CXType_Int:
        {
            init = std::string("scoreboard objectives remove ") + name +
                   "\nscoreboard objectives add " + name + " dummy\n";
            break;
        }
        default:
        {
            std::cerr << "unsupported type: " << clang_getCString(clang_getTypeKindSpelling(type)) << std::endl;
            throw std::runtime_error(std::string("unsupported type: ") + clang_getCString(clang_getTypeKindSpelling(type)));
        }
        }
        return init;
    }

    std::string setVarToVar(std::string name, std::string value, CXTypeKind type)
    {
        std::string set = "";
        switch (type)
        {
        case CXType_Int:
        {
            set = std::string("scoreboard players operation ") + config.name + " " + name + " = " + config.name + " " + value + "\n";
            break;
        }
        default:
        {
            std::cerr << "unsupported type: " << clang_getCString(clang_getTypeKindSpelling(type)) << std::endl;
            throw std::runtime_error(std::string("unsupported type: ") + clang_getCString(clang_getTypeKindSpelling(type)));
        }
        }
        return set;
    }
    std::string setConstToVar(std::string name, std::string value, CXTypeKind type)
    {
        std::string set = "";
        switch (type)
        {
        case CXType_Int:
        {
            set = std::string("scoreboard players set ") + config.name + " " + name + " " + value + "\n";
            break;
        }
        default:
        {
            std::cerr << "unsupported type: " << clang_getCString(clang_getTypeKindSpelling(type)) << std::endl;
            throw std::runtime_error(std::string("unsupported type: ") + clang_getCString(clang_getTypeKindSpelling(type)));
        }
        }
        return set;
    }
    ExprResult start_deal_expr(CXCursor expr)
    {
        tmp_counter = 0;
        return deal_expr(expr);
    }

    ExprResult deal_expr(CXCursor expr)
    {
        // 处理当前节点
        CXCursorKind kind = clang_getCursorKind(expr);
        switch (kind)
        {
        case CXCursor_IntegerLiteral:
        {
            CXEvalResult value = clang_Cursor_Evaluate(expr);
            std::string value_str = std::to_string(clang_EvalResult_getAsInt(value));
            clang_EvalResult_dispose(value);
            int tmp = tmp_counter++;
            if (current_function.name == "Global")
            {
                startFuncitonContent += initVar(std::string("0_tmp_") + std::to_string(tmp), CXType_Int);
                startFuncitonContent += setConstToVar(std::string("0_tmp_") + std::to_string(tmp), value_str, CXType_Int);
            }
            else
            {
                current_content += initVar(std::string("$(functionSpace)_tmp_") + std::to_string(tmp), CXType_Int);
                current_content += setConstToVar(std::string("$(functionSpace)_tmp_") + std::to_string(tmp), value_str, CXType_Int);
            }
            return ExprResult{tmp};
        }
        case CXCursor_UnexposedExpr:
        {
            auto children = getChildCursors(expr);
            if (children.size() == 1)
                return deal_expr(children[0]);
            else
            {
                std::cerr << "unexpected expr: expression has more than one child\n";
                throw std::runtime_error(std::string("unexpected expr: expression has more than one child"));
            }
        }
        case CXCursor_DeclRefExpr:
        {
            CXString name = clang_getCursorSpelling(expr);
            std::string name_str = clang_getCString(name);
            clang_disposeString(name);
            auto itl = localVarsToInt.find(name_str),
                 itg = globalVarsToInt.find(name_str);
            if (itl != localVarsToInt.end())
            {
                Var var = itl->second;
                int tmp = tmp_counter++;

                current_content += initVar(std::string("$(functionSpace)_tmp_" + std::to_string(tmp)), var.type.kind);
                current_content += setVarToVar(std::string("$(functionSpace)_tmp_" + std::to_string(tmp)), std::string("$(functionSpace)_") + std::to_string(var.number), var.type.kind);
                return ExprResult{tmp};
            }
            else if (itg != globalVarsToInt.end())
            {
                Var var = itg->second;
                int tmp = tmp_counter++;

                startFuncitonContent += initVar(std::string("0_tmp_" + std::to_string(tmp)), var.type.kind);
                startFuncitonContent += setVarToVar(std::string("0_tmp_" + std::to_string(tmp)), std::string("0_") + std::to_string(var.number), var.type.kind);
                return ExprResult{tmp};
            }
        }
        default:
        {
            return ExprResult{tmp_counter++};
        }
        }
    }

    void deal_cursor(CXCursor cursor)
    {
        // 处理当前节点
        CXCursorKind kind = clang_getCursorKind(cursor);
        switch (kind)
        {
        case CXCursor_TranslationUnit:
        {
            std::vector<CXCursor> children = getChildCursors(cursor);
            current_function.name = "Global";
            current_function.cursor = cursor;
            startFuncitonContent = initVar("functionSpace", CXType_Int);
            startFuncitonContent += setConstToVar("functionSpace", "1", CXType_Int);

            for (const auto &child : children)
            {
                deal_cursor(child);
            }
            if (functionMap.find("main") != functionMap.end())
            {
                startFuncitonContent +=
                    std::string("execute store result storage ") + config.name + " functionSpace int 1 run scoreboard players get " + config.name + " functionSpace\n" +
                    "function " + config.name + ":" + std::to_string(functionMap["main"].number) + " with storage " +
                    config.name;
            }
            std::ofstream outputFile(function_path / "start.mcfunction", std::ios::out | std::ios::trunc);
            if (!outputFile)
            {
                std::cerr << "open output file error: " << function_path / "start.mcfunction" << std::endl;
                throw std::runtime_error(std::string("open output file error: ") + (function_path / "start.mcfunction").string());
            }
            outputFile << addDollarPrefix(startFuncitonContent);
            outputFile.close();
            break;
        }
        case CXCursor_FunctionDecl:
        {
            CXString name = clang_getCursorSpelling(cursor);
            std::string inside_name = clang_getCString(name);
            CXCursor CompoundStmt = {};
            std::vector<Parm> parms = {};
            std::vector<CXCursor> children = getChildCursors(cursor);
            localVarCounter = 0;
            localVarsToInt.clear();
            bool hasCompoundStmt = false;
            for (const auto &child : children)
            {
                if (clang_getCursorKind(child) == CXCursor_CompoundStmt)
                {
                    CompoundStmt = child;
                    hasCompoundStmt = true;
                }
                else if (clang_getCursorKind(child) == CXCursor_ParmDecl)
                {
                    CXString parm_name = clang_getCursorSpelling(child);
                    std::string parm_name_str = clang_getCString(parm_name);
                    Parm parm{clang_getCursorType(child), parm_name_str, localVarCounter++};
                    parms.push_back(parm);
                    localVarsToInt[parm_name_str] = Var{parm_name_str, parm.kind, parm.number};
                    clang_disposeString(parm_name);
                }
            }
            bool isIntrinsic = std::find(intrinsicFunctions.begin(), intrinsicFunctions.end(), inside_name) != intrinsicFunctions.end();
            if (isIntrinsic && hasCompoundStmt)
            {
                clang_disposeString(name);
                std::cerr << "function " << inside_name << " has been defined" << std::endl;
                throw std::runtime_error(std::string("function ") + inside_name + " has been defined");
            }
            if (functionMap.find(inside_name) == functionMap.end())
            {
                functionMap[inside_name] = Function{inside_name, clang_getCursorResultType(cursor), parms, functionCounter, cursor};
                functionCounter++;
            }
            clang_disposeString(name);
            if (!hasCompoundStmt)
            {
                return; // 如果没有函数体，直接返回
            }

            current_file = fs::path(std::to_string((functionMap[inside_name].number)));
            current_content = "";
            current_function.cursor = cursor;
            current_function.name = std::string(inside_name);
            current_function.number = functionMap[inside_name].number;
            current_function.parms = parms;
            current_function.return_type = clang_getCursorResultType(cursor);
            for (const auto &parm : parms)
            {
                current_content += initVar(std::string("$(functionSpace)_" + std::to_string(parm.number)), parm.kind.kind);
                current_content += setConstToVar(std::string("$(functionSpace)_" + std::to_string(parm.number)), "$(" + std::to_string(parm.number) + ")", parm.kind.kind);
            }
            std::vector<CXCursor> compoundStmtChildren = getChildCursors(CompoundStmt);
            for (const auto &child : compoundStmtChildren)
            {
                deal_cursor(child);
            }
            std::ofstream outputFile(function_path / (current_file.string() + ".mcfunction"), std::ios::out | std::ios::trunc);
            if (!outputFile)
            {
                std::cerr << "open output file error: " << function_path / (current_file.string() + ".mcfunction") << std::endl;
                throw std::runtime_error(std::string("open output file error: ") + (function_path / (current_file.string() + ".mcfunction")).string());
            }
            outputFile << addDollarPrefix(current_content);
            outputFile.close();

            // 恢复函数空间
            current_function.name = "Global";
            current_function.cursor = cursor;
            break;
        }
        case CXCursor_DeclStmt:
        {
            std::vector<CXCursor> children = getChildCursors(cursor);
            for (const auto &child : children)
            {
                deal_cursor(child);
            }
            break;
        }
        case CXCursor_VarDecl:
        {
            CXType type = clang_getCursorType(cursor);
            CXString name = clang_getCursorSpelling(cursor);
            std::string name_str = clang_getCString(name);
            clang_disposeString(name);
            if (current_function.name == "Global")
            {
                int number = globalVarCounter;
                globalVarsToInt[name_str] = Var{name_str, type, globalVarCounter++};
                CXCursor initializer = clang_Cursor_getVarDeclInitializer(cursor);
                bool hasInitializer = false;
                ExprResult exprResult;
                if (!clang_equalCursors(initializer, clang_getNullCursor()))
                {
                    hasInitializer = true;
                    exprResult = start_deal_expr(initializer);
                }
                if (!hasInitializer)
                {
                    startFuncitonContent += initVar(std::string("0_" + std::to_string(number)), type.kind);
                    return;
                }

                startFuncitonContent += initVar(std::string("0_" + std::to_string(number)), type.kind);
                startFuncitonContent += setVarToVar(std::string("0_" + std::to_string(number)), std::string("0_tmp_") + std::to_string(exprResult.tmp_number), type.kind);
            }
            else
            {
                int number = localVarCounter;
                localVarsToInt[name_str] = Var{name_str, type, localVarCounter++};
                CXCursor initializer = clang_Cursor_getVarDeclInitializer(cursor);
                bool hasInitializer = false;
                ExprResult exprResult;
                if (!clang_equalCursors(initializer, clang_getNullCursor()))
                {
                    hasInitializer = true;
                    exprResult = start_deal_expr(initializer);
                }
                if (!hasInitializer)
                {
                    current_content += initVar(std::string("$(functionSpace)_" + std::to_string(number)), type.kind);
                    return;
                }
                current_content += initVar(std::string("$(functionSpace)_" + std::to_string(number)), type.kind);
                current_content += setVarToVar(std::string("$(functionSpace)_" + std::to_string(number)), std::string("$(functionSpace)_tmp_") + std::to_string(exprResult.tmp_number), type.kind);
            }
            break;
        }
        case CXCursor_ReturnStmt:
        {
            std::vector<CXCursor> children = getChildCursors(cursor);
            switch (current_function.return_type.kind)
            {
            case CXType_Void:
            {
                current_content += "return 0\n";
                break;
            }
            case CXType_Int:
            {
                if (children.size() == 1)
                {
                    ExprResult exprResult = start_deal_expr(children[0]);

                    current_content += initVar("return", CXType_Int);
                    current_content += setVarToVar("return", std::string("$(functionSpace)_tmp_") + std::to_string(exprResult.tmp_number), CXType_Int);
                    current_content += "return 0\n";
                }
                break;
            }
            default:
            {
                if (std::find(exprStatementKinds.begin(), exprStatementKinds.end(), kind) != exprStatementKinds.end())
                {
                    start_deal_expr(cursor); // 表达式语句：求值副作用，丢弃结果
                }
                break;
            }
            }
        }
        default:
        {
            break;
        }
        }
    }

    void compile(CXCursor root_cursor)
    {
        fs::path build = fs::current_path() / "build";
        fs::path output_path = build / config.name;

#ifdef _DEBUG
        std::cout << "Building " << config.name << std::endl;
        std::cout << "Build path: " << build << std::endl;
        std::cout << "Output path: " << output_path << std::endl;
#endif
        std::cout << "clear output file: " << output_path << std::endl;
        try
        {
            if (fs::remove_all(output_path))
            {
                std::cout << "clear output file success: " << output_path << std::endl;
            }
            else
            {
                std::cout << "output file " << output_path << " is cleared" << std::endl;
            }
        }
        catch (const fs::filesystem_error &e)
        {
            std::cerr << "clear output file error: " << e.what() << std::endl;
        }
        bool created = fs::create_directories(output_path);
        if (!created)
        {
            std::cout << "create output directory error: " << output_path << std::endl;
            throw std::runtime_error(std::string("create output directory error: ") + output_path.string());
        }
        std::ofstream outputFile(output_path / "pack.mcmeta", std::ios::out | std::ios::trunc);
        if (!outputFile)
        {
            std::cerr << "open output file error: " << output_path << std::endl;
            throw std::runtime_error(std::string("open output file error: ") + output_path.string());
        }
        outputFile << "{\n"
                   << "    \"pack\": {\n"
                   << "        \"pack_format\": " << config.pack_format << ",\n"
                   << "        \"description\": \"" << config.description << "\"\n"
                   << "    }\n"
                   << "}";
        outputFile.close();
        created = fs::create_directories(output_path / "data" / config.name / "function");
        if (!created)
        {
            std::cout << "create output directory error: " << output_path / "data" / config.name / "function" << std::endl;
            throw std::runtime_error(std::string("create output directory error: ") + (output_path / "data" / config.name / "function").string());
        }
        function_path = output_path / "data" / config.name / "function";
        deal_cursor(root_cursor);
    }
} // namespace craftlang
