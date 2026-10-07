#include <sim/tokenize.hpp>
#include <optional>
#include <iostream>

namespace craftsim
{
    TokenizeContext initTokenizeContext(std::string_view &function)
    {
        TokenizeContext ret;
        ret.pos = std::pair<int, int>(1, 0);
        ret.function = function;
        ret.rowStarts = std::vector<int>();
        ret.rowStarts.push_back(-1);
        ret.rowStarts.push_back(0);
        return ret;
    }
    void tokenize(TokenizeContext &context, Token &token)
    {
        std::string_view &function = context.function;
        std::optional<std::pair<int, int>> start = std::nullopt, end = std::nullopt;
        bool inString = false;
        while (true)
        {
            int location = context.rowStarts[context.pos.first] + context.pos.second;
            if (location >= (int)function.size())
            {
                if (start.has_value())
                {
                    if (inString)
                    {
                        token.tokentype = TOK_ERROR;
                        return;
                    }
                    end = context.pos;
                    end.value().second--;
                    token.start = start.value();
                    token.end = end.value();
                    token.tokentype = TOK_WORD;
                    int startLocation = context.rowStarts[start.value().first] + start.value().second;
                    token.string = function.substr(startLocation, location - startLocation);
                    return;
                }
                else
                {
                    token.start = std::pair<int, int>(0, 0);
                    token.end = std::pair<int, int>(0, 0);
                    token.string = "";
                    token.tokentype = TOK_END;
                    return;
                }
            }
            char c = function[location];
            if (c == '"')
            {
                if (!inString)
                {
                    if (!start.has_value())
                    {
                        start = context.pos;
                        context.pos.second++;
                        inString = true;
                        continue;
                    }
                    else
                    {
                        token.tokentype = TOK_ERROR;
                        return;
                    }
                }
                else
                {
                    end = context.pos;
                    context.pos.second++;
                    token.start = start.value();
                    token.end = end.value();
                    token.tokentype = TOK_STRING;
                    int startLocation = context.rowStarts[start.value().first] + start.value().second;
                    token.string = function.substr(startLocation, location + 1 - startLocation);
                    return;
                }
            }
            if (!inString)
            {
                if ((c == ' ' || c == '\n'))
                {
                    if (start.has_value())
                    {
                        end = context.pos;
                        end.value().second--;
                        token.start = start.value();
                        token.end = end.value();
                        token.tokentype = TOK_WORD;
                        int startLocation = context.rowStarts[start.value().first] + start.value().second;
                        token.string = function.substr(startLocation, location - startLocation);
                        return;
                    }
                }
                if (c == '\n')
                {
                    token.start = context.pos;
                    token.end = context.pos;
                    token.string = function.substr(location, 1);
                    token.tokentype = TOK_NEWLINE;
                    context.pos.first++;
                    context.pos.second = 0;
                    context.rowStarts.push_back(location + 1);
                    return;
                }
                if (c == ' ')
                {

                    context.pos.second++;
                    continue;
                }
                if (!start.has_value())
                    start = context.pos;
            }
            else
            {
                if (c == '\n')
                {
                    token.tokentype = TOK_ERROR;
                    return;
                }
            }
            context.pos.second++;
        }
    }
} // namespace craftsim
