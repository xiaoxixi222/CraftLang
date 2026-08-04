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
    const std::vector<std::string> intrinsicFunctions = {
        "print_int",
    };
    std::unordered_map<std::string, Function> functionMap = {};
    int functionCounter = 0;

    std::string current_content = "", startFuncitonContent = "";
    fs::path current_file = "";
    fs::path function_path = "";
    Function current_function;
    std::unordered_map<std::string, int> localVarsToInt = {};  // 局部变量的编号
    std::unordered_map<std::string, int> globalVarsToInt = {}; // 全局变量的编号
    int localVarCounter = 0, globalVarCounter = 0;

    static void deal_cursor(CXCursor cursor)
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
            startFuncitonContent = std::string("scoreboard objectives remove functionSpace\n") +
                                   "scoreboard objectives add functionSpace dummy\n" +
                                   "scoreboard players set " + config.name + " functionSpace 0\n";

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
            outputFile << startFuncitonContent;
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
                    localVarsToInt[parm_name_str] = parm.number;
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
            for (const auto &parm : parms)
            {
                switch (parm.kind.kind)
                {
                case CXType_Int:
                {
                    current_content += "$scoreboard objectives remove $(functionSpace)" + std::to_string(parm.number) +
                                       "\n$scoreboard objectives add $(functionSpace)" + std::to_string(parm.number) + " dummy\n" +
                                       "$scoreboard players set " + config.name + " $(functionSpace)" + std::to_string(parm.number) + " $(" + std::to_string(parm.number) + ")\n";
                    break;
                }
                default:
                {
                    std::cerr << "unsupported type: " << inside_name << std::endl;
                    throw std::runtime_error(std::string("unsupported type: ") + inside_name);
                }
                }
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
            outputFile << current_content;
            outputFile.close();

            // 恢复函数空间
            current_function.name = "Global";
            current_function.cursor = cursor;
            break;
        }
        case CXCursor_DeclStmt:
        {
            break;
        }
        default:
            break;
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
