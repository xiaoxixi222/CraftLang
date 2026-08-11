#include <craftlang/lang.h>
#include <craftlang/compiler.h>
#include <craftlang/config.h>

#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <filesystem>

#include <clang-c/Index.h> // libclang 头文件
#include <json.hpp>        // nlohmann/json 头文件

using json = nlohmann::json;
namespace fs = std::filesystem; // 使用 std::filesystem 库

craftlang::Config config; // 使用 craftlang 的配置结构体

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        std::cerr << "Usage: craftlang <source-file.c>\n";
        return 1;
    }

    std::string filePath = argv[1];
    fs::path filePath2 = argv[1];
    if (argc == 3)
    {
        config.output_path = argv[2];
    }
    else
    {
        config.output_path = fs::current_path() / "build"; // 默认输出路径为当前工作目录
    }
    std::cout << "Input file: " << filePath << "\n";

    config.name = "..name..";
    config.file_name = filePath2.stem().string();

    // libclang 解析与 AST 遍历
    // 1. 创建索引
    CXIndex index = clang_createIndex(0, 0);

    // 2. 解析源文件，生成翻译单元（Translation Unit）
    //    加入 -I 参数，使 libclang 在解析时能定位项目 include/ 下的头文件（如 mcstd.h）
    std::vector<std::string> args;
    args.push_back("-I");
    args.push_back(CRAFTLANG_SRC_INCLUDE);
    // 暂时在考虑那些内置函数是通过这个方式剔除呢还是在ast里删掉。
    // args.push_back("-D");
    // args.push_back("CRAFTLANG");
#ifdef _DEBUG
    std::cout << "Include path: " << CRAFTLANG_SRC_INCLUDE << std::endl;
#endif

    std::vector<const char *> clangArgs;
    clangArgs.reserve(args.size());
    for (const auto &s : args)
        clangArgs.push_back(s.c_str());

    CXTranslationUnit unit =
        clang_parseTranslationUnit(index,
                                   filePath.c_str(), // 要解析的文件
                                   clangArgs.data(), // 命令行参数
                                   static_cast<int>(clangArgs.size()),
                                   nullptr, 0, // 无需额外文件
                                   CXTranslationUnit_None);

    if (unit == nullptr)
    {
        std::cerr << "解析失败" << std::endl;
        return 1;
    }

    // 2.5 打印 libclang 诊断信息（错误/警告），便于定位解析问题
    unsigned diagCount = clang_getNumDiagnostics(unit);
    for (unsigned i = 0; i < diagCount; ++i)
    {
        CXDiagnostic diag = clang_getDiagnostic(unit, i);
        CXString msg = clang_formatDiagnostic(diag,
                                              clang_defaultDiagnosticDisplayOptions());
        std::cerr << clang_getCString(msg) << std::endl;
        clang_disposeString(msg);
        clang_disposeDiagnostic(diag);
    }
    if (diagCount > 0)
    {
        throw std::runtime_error("parse error\n"); // 解析失败，抛出异常
    }

    // 3. 获取翻译单元的根游标（Cursor）
    CXCursor rootCursor = clang_getTranslationUnitCursor(unit);

    // 4. 遍历AST
    craftlang::all_cursors[rootCursor] =
        craftlang::Cursor{rootCursor, rootCursor, std::vector<CXCursor>()}; // 初始化根节点
#ifdef _DEBUG
    std::cout << "root node: "
              << clang_getCString(
                     clang_getCursorKindSpelling(clang_getCursorKind(rootCursor)))
              << std::endl;
#endif
    clang_visitChildren(rootCursor, craftlang::collectChildrenCallback,
                        nullptr); // 先收集子节点
    craftlang::visitCursorRecursive(rootCursor, 0);
    craftlang::compile(rootCursor);

    // 5. 释放资源
    clang_disposeTranslationUnit(unit);
    clang_disposeIndex(index);

    return 0;
}
