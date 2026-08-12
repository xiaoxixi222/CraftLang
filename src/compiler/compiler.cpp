#include <craftlang/compiler.h>
#include <craftlang/lang.h>
#include <craftlang/config.h>
#include <json.hpp> // nlohmann/json 头文件

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>

using json = nlohmann::json;
namespace fs = std::filesystem;
extern craftlang::Config config; // 使用 craftlang 的配置结构体
namespace craftlang
{
    const std::vector<CXBinaryOperatorKind> binaryOperatorNeedLValue = {
        CXBinaryOperator_Assign,
    };
    std::unordered_map<std::string, Function> functionMap = {};
    int functionCounter = 0;

    std::string current_content = "", startFunctionContent = "";
    fs::path current_file = "";
    fs::path function_path = "";
    Function current_function;
    std::unordered_map<std::string, Var> localVarsToInt = {};  // 局部变量的编号
    std::unordered_map<std::string, Var> globalVarsToInt = {}; // 全局变量的编号
    int localVarCounter = 0, globalVarCounter = 0;

    int tmp_counter = 0;
    const std::vector<CXCursorKind> exprStatementKinds = {
        CXCursor_CallExpr,       // print_int(c); add(a,b);
        CXCursor_BinaryOperator, // d = c;
        CXCursor_ParenExpr,
    };

    json symbols = json::object();

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
    const Var &find_var(std::string name)
    {
        auto itl = localVarsToInt.find(name),
             itg = globalVarsToInt.find(name);
        if (itl != localVarsToInt.end())
        {
            return itl->second;
        }
        if (itg != globalVarsToInt.end())
        {
            return itg->second;
        }

        std::cerr << "variable not found: " << name << std::endl;
        throw std::runtime_error(std::string("variable not found: ") + name);
    }
    void addCommand(std::string cmd)
    {
        if (current_function.name == "Global")
        {
            replaceAll(cmd, "?(space)", "0");
            startFunctionContent += cmd;
        }
        else
        {
            replaceAll(cmd, "?(space)", "$(functionSpace)");
            current_content += cmd;
        }
    }
    LValue deal_lvalue(CXCursor lvalue)
    {
        CXCursorKind kind = clang_getCursorKind(lvalue);
        switch (kind)
        {
        case CXCursor_DeclRefExpr:
        {
            CXString name = clang_getCursorSpelling(lvalue);
            std::string name_str = clang_getCString(name);
            clang_disposeString(name);
            Var var = find_var(name_str);
            return LValue{var.objective};
        }
        case CXCursor_ParenExpr:
        case CXCursor_UnexposedExpr:
        {
            auto children = getChildCursors(lvalue);
            if (children.size() == 1)
                return deal_lvalue(children[0]);
            else
            {
                std::cerr << "unexpected expr: expression has more than one child\n";
                throw std::runtime_error(std::string("unexpected expr: expression has more than one child"));
            }
        }
        default:
        {
            break;
        }
        }
        return LValue{}; // 默认返回空值
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
            addCommand(initVar("?(space)_tmp_" + std::to_string(tmp), CXType_Int) +
                       setConstToVar(std::string("?(space)_tmp_") + std::to_string(tmp), value_str, CXType_Int));
            return ExprResult{tmp};
        }
        case CXCursor_ParenExpr:
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
            int tmp = tmp_counter++;
            Var var = find_var(name_str);
            addCommand(initVar(std::string("?(space)_tmp_") + std::to_string(tmp), var.type.kind) +
                       setVarToVar(std::string("?(space)_tmp_") + std::to_string(tmp), var.objective, var.type.kind));
            return ExprResult{tmp};
        }
        case CXCursor_BinaryOperator:
        {
            auto children = getChildCursors(expr);
            if (children.size() != 2)
            {
                std::cerr << "unexpected expr: binary operator has more than two children\n";
                throw std::runtime_error(std::string("unexpected expr: binary operator has more than two children"));
            }
            CXBinaryOperatorKind kind = clang_getCursorBinaryOperatorKind(expr);
            if (std::find(binaryOperatorNeedLValue.begin(),
                          binaryOperatorNeedLValue.end(), kind) != binaryOperatorNeedLValue.end())
            {
                LValue left = deal_lvalue(children[0]);
                ExprResult right = deal_expr(children[1]);
                int tmp = right.tmp_number;
                switch (kind)
                {
                case CXBinaryOperator_Assign:
                {
                    addCommand(std::string("scoreboard players operation ") + config.name + " " + left.objective + " = " + config.name + " ?(space)_tmp_" + std::to_string(right.tmp_number) + "\n");
                }
                default:
                {
                    break;
                }
                }
                return ExprResult{tmp};
            }
            ExprResult left = deal_expr(children[0]);
            ExprResult right = deal_expr(children[1]);
            tmp_counter--;
            int tmp = left.tmp_number;
            switch (kind)
            {
            case CXBinaryOperator_Add:
            {
                addCommand(std::string("scoreboard players operation ") + config.name + " ?(space)_tmp_" + std::to_string(left.tmp_number) + " += " + config.name + " ?(space)_tmp_" + std::to_string(right.tmp_number) + "\n");
                break;
            }
            case CXBinaryOperator_Mul:
            {
                addCommand(std::string("scoreboard players operation ") + config.name + " ?(space)_tmp_" + std::to_string(left.tmp_number) + " *= " + config.name + " ?(space)_tmp_" + std::to_string(right.tmp_number) + "\n");
                break;
            }
            case CXBinaryOperator_Sub:
            {
                addCommand(std::string("scoreboard players operation ") + config.name + " ?(space)_tmp_" + std::to_string(left.tmp_number) + " -= " + config.name + " ?(space)_tmp_" + std::to_string(right.tmp_number) + "\n");
                break;
            }
            case CXBinaryOperator_Div:
            {
                addCommand(std::string("scoreboard players operation ") + config.name + " ?(space)_tmp_" + std::to_string(left.tmp_number) + " /= " + config.name + " ?(space)_tmp_" + std::to_string(right.tmp_number) + "\n");
                break;
            }
            case CXBinaryOperator_Rem:
            {
                addCommand(std::string("scoreboard players operation ") + config.name + " ?(space)_tmp_" + std::to_string(left.tmp_number) + " %= " + config.name + " ?(space)_tmp_" + std::to_string(right.tmp_number) + "\n");
                break;
            }
            default:
            {
                break;
            }
            }
            return ExprResult{tmp};
        }
        case CXCursor_UnaryOperator:
        {
            switch (clang_getCursorUnaryOperatorKind(expr))
            {
            case CXUnaryOperator_Plus: // +x：恒等，直接透传
                return deal_expr(getChildCursors(expr)[0]);
            case CXUnaryOperator_Minus: // -x：求值后取负
            {
                ExprResult operand = deal_expr(getChildCursors(expr)[0]);
                int tmp = operand.tmp_number;
                // MC 没有一元取负，用 0 - x
                addCommand(initVar("const-1", CXType_Int) + setConstToVar(std::string("const-1"), "-1", CXType_Int) +
                           "scoreboard players operation " + config.name + " ?(space)_tmp_" + std::to_string(tmp) + " *= " + config.name + " const-1\n");
                return ExprResult{tmp};
            }
            default:
                std::cerr << "unsupported unary operator\n";
                throw std::runtime_error("unsupported unary operator"); // ++/--/&/*/! 等：响亮失败，别静默
            }
        }
        case CXCursor_CallExpr:
        {
            CXCursor function = clang_getCursorReferenced(expr);
            CXString function_name = clang_getCursorSpelling(function);
            std::string function_name_str = clang_getCString(function_name);
            clang_disposeString(function_name);
            Function functionVar = functionMap[function_name_str];
            int num = clang_Cursor_getNumArguments(expr);
            std::vector<ExprResult> args;
            for (int i = 0; i < num; i++)
            {
                ExprResult arg = deal_expr(clang_Cursor_getArgument(expr, i));
                addCommand(std::string("execute store result storage ") + config.name + " " + std::to_string(i) + " int 1 run scoreboard players get " + config.name + " ?(space)_tmp_" + std::to_string(arg.tmp_number) + "\n");
            }
            int tmp = tmp_counter - num + 1;

            if (functionVar.return_type.kind != CXType_Void)
            {
                tmp_counter = tmp + 1;
                addCommand(std::string("scoreboard players add ") + config.name + " functionSpace 1\n" +
                           "execute store result storage " + config.name + " functionSpace int 1 run scoreboard players get " + config.name + " functionSpace\n" +
                           "function " + config.name + ":.f." + std::to_string(functionVar.number) + ".f. with storage " + config.name +
                           "\nscoreboard players remove " + config.name + " functionSpace 1\n");
                addCommand(initVar(std::string("?(space)_tmp_") + std::to_string(tmp), functionVar.return_type.kind) +
                           setVarToVar(std::string("?(space)_tmp_") + std::to_string(tmp), std::string("return"), functionVar.return_type.kind));
            }
            else
            {
                tmp_counter = tmp;
                addCommand(std::string("scoreboard players add ") + config.name + " functionSpace 1\n" +
                           "execute store result storage " + config.name + " functionSpace int 1 run scoreboard players get " + config.name + " functionSpace\n" +
                           "function " + config.name + ":.f." + std::to_string(functionVar.number) + ".f. with storage " + config.name +
                           "\nscoreboard players remove " + config.name + " functionSpace 1\n");
            }
            return ExprResult{tmp};
        }
        default:
        {
            std::cerr << "unsupported expr: " << clang_getCString(clang_getCursorKindSpelling(clang_getCursorKind(expr))) << std::endl;
            throw std::runtime_error(std::string("unsupported expr: ") + clang_getCString(clang_getCursorKindSpelling(clang_getCursorKind(expr))));
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
            for (const auto &child : children)
            {
                deal_cursor(child);
            }
            std::ofstream outputFile(function_path / "start.mcfunction", std::ios::out | std::ios::trunc);
            if (!outputFile)
            {
                std::cerr << "open output file error: " << function_path / "start.mcfunction" << std::endl;
                throw std::runtime_error(std::string("open output file error: ") + (function_path / "start.mcfunction").string());
            }
            symbols["Global"] = json::array();
            symbols["function"] = json::array();
            for (const auto &pair : globalVarsToInt)
            {
                Var var = pair.second;
                json var_json = json::object();
                var_json["name"] = var.name;
                var_json["number"] = var.number;
                var_json["isExtern"] = var.isExtern;
                symbols["Global"].push_back(var_json);
            }
            for (const auto &pair : functionMap)
            {
                Function function = pair.second;
                json function_json = json::object();
                function_json["name"] = function.name;
                function_json["number"] = function.number;
                function_json["isExtern"] = function.isExtern;
                symbols["function"].push_back(function_json);
            }
            outputFile << addDollarPrefix(startFunctionContent);
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
                    localVarsToInt[parm_name_str] = Var{parm_name_str, std::string("$(functionSpace)_") + std::to_string(parm.number), parm.kind, parm.number, false};
                    clang_disposeString(parm_name);
                }
            }
            if (functionMap.find(inside_name) == functionMap.end())
            {
                functionMap[inside_name] = Function{inside_name, clang_getCursorResultType(cursor), parms, functionCounter, cursor, true};
                functionCounter++;
            }
            clang_disposeString(name);
            if (!hasCompoundStmt)
            {
                return; // 如果没有函数体且不是内建函数，直接返回
            }

            current_file = fs::path(std::to_string((functionMap[inside_name].number)));
            current_content = "";
            current_function.cursor = cursor;
            current_function.name = std::string(inside_name);
            current_function.number = functionMap[inside_name].number;
            current_function.parms = parms;
            current_function.return_type = clang_getCursorResultType(cursor);
            current_function.isExtern = false;
            functionMap[inside_name].isExtern = false;
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
            /*
            json symbolTable = json::array();
            for (const auto &pair : localVarsToInt)
            {
                Var var = pair.second;
                CXString typeName = clang_getTypeKindSpelling(var.type.kind);
                std::string typeName_str = clang_getCString(typeName);
                clang_disposeString(typeName);
                symbolTable.push_back(json{var.name, var.objective, typeName_str});
            }
            symbols[inside_name] = symbolTable;*/
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
            CX_StorageClass storageClass = clang_Cursor_getStorageClass(cursor);
            clang_disposeString(name);
            if (current_function.name == "Global")
            {
                int number = globalVarCounter;
                globalVarsToInt[name_str] = Var{name_str, std::string("0_.g.") + std::to_string(number) + ".g.", type, globalVarCounter++, storageClass == CX_SC_Extern};
                if (storageClass == CX_SC_Extern) return;
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
                    startFunctionContent += initVar(std::string("0_.g." + std::to_string(number) + ".g."), type.kind);
                    return;
                }
                startFunctionContent += initVar(std::string("0_.g." + std::to_string(number) + ".g."), type.kind);
                startFunctionContent += setVarToVar(std::string("0_.g." + std::to_string(number) + ".g."), std::string("0_tmp_") + std::to_string(exprResult.tmp_number), type.kind);
            }
            else
            {
                int number = localVarCounter++;
                localVarsToInt[name_str] = Var{name_str, std::string("$(functionSpace)_") + std::to_string(number), type, number, false};
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

                break;
            }
            }
            break;
        }
        default:
        {
            if (std::find(exprStatementKinds.begin(), exprStatementKinds.end(), kind) != exprStatementKinds.end())
            {
                start_deal_expr(cursor); // 表达式语句：求值副作用，丢弃结果
            }
            else
            {
                std::cerr << "unsupported cursor: " << clang_getCString(clang_getCursorKindSpelling(clang_getCursorKind(cursor))) << std::endl;
                throw std::runtime_error(std::string("unsupported cursor: ") + clang_getCString(clang_getCursorKindSpelling(clang_getCursorKind(cursor))));
            }
        }
        }
    }

    void compile(CXCursor root_cursor)
    {
        fs::path build = config.output_path / (config.file_name + "_o");
        fs::path symbols_path = build.parent_path() / (config.file_name + "_symbols.json");
        function_path = build / "function";
        fs::remove_all(build);
        fs::create_directories(function_path);
        deal_cursor(root_cursor);
        // 保存符号表
        std::ofstream symbolsFile(symbols_path, std::ios::out | std::ios::trunc);
        if (!symbolsFile)
        {
            std::cerr << "open output file error: " << symbols_path << std::endl;
            throw std::runtime_error(std::string("open output file error: ") + symbols_path.string());
        }
        symbolsFile << symbols.dump(4) << std::endl;
        symbolsFile.close();
    }
} // namespace craftlang
