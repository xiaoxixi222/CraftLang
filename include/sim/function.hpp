#pragma once
#include <string>
#include <vector>
#include <optional>
#include <sim/storage.hpp>
namespace craftsim
{
    struct Function
    {
        std::string name;
        std::string content;
    };
    enum TokenType
    {
        TOK_WORD,
        TOK_END,
        TOK_NEWLINE,
        TOK_STRING,
        TOK_ERROR
    };
    struct Token
    {
        std::pair<int, int> start;
        std::pair<int, int> end;
        std::string_view string;
        TokenType tokentype;
    };
    struct TokenizeContext
    {
        std::pair<int, int> pos;
        std::string_view function;
        std::vector<int> rowStarts;
    };
    TokenizeContext initTokenizeContext(std::string_view &function);
    void tokenize(TokenizeContext &context, Token &token);

    // 把函数文本里的 $(key) 宏用 source 对应的 NBT 数据展开，返回处理后的字符串。
    // 不做任何执行，只做纯字符串变换。
    std::string dueWithFunction(std::string function,
                                std::optional<std::string> source = std::nullopt,
                                std::optional<NbtPath> path = std::nullopt);
    void executeFunction(const Function &function, const std::optional<std::string> &source, const std::optional<NbtPath> &path);
} // namespace craftsim