#pragma once
#include <string>
#include <string_view>
#include <vector>
#include <optional>
#include <map>
#include <functional>
#include <utility>
#include <sim/storage.hpp>
namespace craftsim
{
    struct Function
    {
        std::string name;
        std::string content;
    };
    struct LogEntry
    {
        std::pair<int, int> position;
        std::string message;
    };
    /*returnValue
    successFlag
    isReturn
    out
    error
    warn
    info
    */
    struct ReturnState
    {
        int returnValue;
        bool successFlag;
        bool isReturn;
        std::string out;
        std::vector<LogEntry> error;
        std::vector<LogEntry> warn;
        std::vector<LogEntry> info;
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
    enum TokenizeType
    {
        TOKENIZE_NORMAL, // 正常
        TOKENIZE_LINE    /*从当前位置截到行尾（跳过行首空格，不含 \r/\n），不消费换行符；换行由 TOKENIZE_NORMAL 产生*/
    };
    struct DueWithFunctionResult
    {
        std::string processedFunction;
        bool hasError;
        std::string errorMessage;
    };
    TokenizeContext initTokenizeContext(std::string_view &function);
    void tokenize(TokenizeContext &context, Token &token);
    void tokenize(TokenizeContext &context, Token &token, TokenizeType type);

    // 把函数文本里的 $(key) 宏用 source 对应的 NBT 数据展开，返回处理后的字符串。
    // 不做任何执行，只做纯字符串变换。
    DueWithFunctionResult dueWithFunction(std::string function,
                                          std::optional<std::string> source = std::nullopt,
                                          std::optional<NbtPath> path = std::nullopt);
    ReturnState executeFunction(const Function &function, const std::optional<std::string> &source, const std::optional<NbtPath> &path);

    using CommandFunc = ReturnState (*)(Token &, TokenizeContext &);
    extern std::map<std::string, craftsim::CommandFunc, std::less<>> commandToFunction;
    namespace commandRunner
    {
        ReturnState runSay(Token &token, TokenizeContext &context);
        ReturnState runReturn(Token &token, TokenizeContext &context);
        ReturnState runScoreboard(Token &token, TokenizeContext &context);
        ReturnState runExecute(Token &token, TokenizeContext &context);
        ReturnState runFunction(Token &token, TokenizeContext &context);
    } // namespace commandRunner
} // namespace craftsim
