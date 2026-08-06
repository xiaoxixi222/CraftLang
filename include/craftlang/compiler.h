#ifndef CRAFTLANG_COMPILER_H
#define CRAFTLANG_COMPILER_H

#include <clang-c/Index.h>

#include <string>
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

    struct Var{
        std::string name;
        CXType type;
        int number;
    };

    void compile(CXCursor root_cursor);

} // namespace craftlang

#endif // CRAFTLANG_COMPILER_H
