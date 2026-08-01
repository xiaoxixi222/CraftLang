#include <iostream>
#include <string>

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        std::cerr << "Usage: craftlang <source-file.cl>\n";
        return 1;
    }

    std::string filePath = argv[1];
    std::cout << "Input file: " << filePath << "\n";

    // TODO: libclang 解析与 AST 遍历
    return 0;
}
