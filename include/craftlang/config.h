#ifndef CRAFTLANG_CONFIG_H
#define CRAFTLANG_CONFIG_H
#include <string>
namespace craftlang {

struct Config
{
    std::string name;
    int pack_format;
    std::string description;
};

} // namespace craftlang

#endif // CRAFTLANG_CONFIG_H
