#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

#include "cppparser/cppparser.h"

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        std::cerr << "Usage: craftlang <source-file.cl>\n";
        return 1;
    }

    std::string filePath = argv[1];

    std::ifstream file(filePath);
    if (!file.is_open())
    {
        std::cerr << "Error: Cannot open file: " << filePath << "\n";
        return 1;
    }

    cppparser::CppParser parser;
    auto ast = parser.parseFile(filePath);

    if (!ast)
    {
        std::cerr << "Error: Failed to parse source file.\n";
        return 1;
    }

    std::cout << "Parse successful.\n";
    std::cout << "File: " << ast->name() << "\n";
    std::cout << "Top-level compound type: "
              << static_cast<int>(ast->compoundType()) << "\n";

    return 0;
}
