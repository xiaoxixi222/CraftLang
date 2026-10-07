#pragma once
#include <string>
#include <vector>
namespace craftsim
{
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
} // namespace craftsim