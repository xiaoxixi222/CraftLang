#include <sim/function.hpp>
#include <sim/sim.hpp>
#include <optional>
#include <iostream>
#include <sstream>
#include <map>

namespace craftsim
{
    std::map<std::string, craftsim::CommandFunc, std::less<>> commandToFunction = {
        {"return", commandRunner::runReturn},
        {"scoreboard", (CommandFunc) nullptr},
        {"execute", (CommandFunc) nullptr},
        {"function", (CommandFunc) nullptr},
        {"say", commandRunner::runSay},
        {"data", (CommandFunc) nullptr},
    }; // 指令->函数映射表
    namespace
    {
        // 判断字符串是否是纯整数（可选正负号，后面全是数字）
        bool isPureInteger(const std::string_view &str)
        {
            if (str.empty())
                return false; // 空字符串不是数字

            // 处理正负号（可选）
            size_t i = 0;
            if (str[0] == '+' || str[0] == '-')
            {
                if (str.length() == 1)
                    return false; // 只有一个符号也不行
                i = 1;
            }

            for (; i < str.length(); ++i)
            {
                // 判断字符是否在 '0' 到 '9' 之间
                if (str[i] < '0' || str[i] > '9')
                {
                    return false;
                }
            }
            return true;
        }
        // 判断一行里是否有 $(...) 形态的宏引用
        bool has_macro(const std::string &line)
        {
            size_t i = 0;
            while (i + 1 < line.size())
            {
                if (line[i] == '$' && line[i + 1] == '(')
                {
                    return true;
                }
                ++i;
            }
            return false;
        }

        // 把 NBT 节点转成字符串，只接受可直接变字符串的类型；
        // list / compound / array / end 返回 nullopt，跳过
        std::optional<std::string> nbt_to_macro_string(const NBT &v)
        {
            if (v.is_string())
                return std::get<std::string>(v.value);
            if (v.is_byte())
                return std::to_string(static_cast<int>(std::get<int8_t>(v.value)));
            if (v.is_short())
                return std::to_string(std::get<int16_t>(v.value));
            if (v.is_int())
                return std::to_string(std::get<int32_t>(v.value));
            if (v.is_long())
                return std::to_string(std::get<int64_t>(v.value));
            if (v.is_float())
                return std::to_string(std::get<float>(v.value));
            if (v.is_double())
                return std::to_string(std::get<double>(v.value));
            return std::nullopt;
        }

        void collect_macros(const NBT_Map &map, std::map<std::string, std::string> &macros)
        {
            for (const auto &[k, v] : map)
            {
                auto s = nbt_to_macro_string(v);
                if (s.has_value())
                {
                    macros[k] = *s;
                }
            }
        }

        // 把一行里的 $(key) 替换成 macros[key]，找不到的保留原样
        std::string expand_line(const std::string &line, const std::map<std::string, std::string> &macros)
        {
            std::string result;
            result.reserve(line.size());

            size_t i = 0;
            while (i < line.size())
            {
                if (line[i] == '$' && i + 1 < line.size() && line[i + 1] == '(')
                {
                    size_t close = line.find(')', i + 2);
                    if (close != std::string::npos)
                    {
                        std::string key = line.substr(i + 2, close - i - 2);
                        auto it = macros.find(key);
                        if (it != macros.end())
                        {
                            result += it->second;
                        }
                        else
                        {
                            result.append(line, i, close - i + 1); // 保留 $(key)
                        }
                        i = close + 1;
                        continue;
                    }
                }
                result += line[i++];
            }
            return result;
        }

    } // namespace
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
                if ((c == ' ' || c == '\r' || c == '\n'))
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
                if (c == '\r')
                {
                    // CRLF：跳过 \r，由后面的 \n 生成唯一的换行 token
                    if (location + 1 < (int)function.size() && function[location + 1] == '\n')
                    {
                        context.pos.second++;
                        continue;
                    }
                    // 单独的 \r 也按换行处理
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
                if (c == '\n' || c == '\r')
                {
                    token.tokentype = TOK_ERROR;
                    return;
                }
            }
            context.pos.second++;
        }
    }
    DueWithFunctionResult dueWithFunction(std::string function, std::optional<std::string> source, std::optional<NbtPath> path)
    {
        // 第一步：扫描整个函数，看有没有需要展开的宏
        bool need_macro = false;
        {
            std::istringstream scan(function);
            std::string line;
            while (std::getline(scan, line))
            {
                if (!line.empty() && line[0] == '$' && has_macro(line))
                {
                    need_macro = true;
                    break;
                }
            }
        }

        // 没有宏需要展开 → 原样返回，不需要 source
        if (!need_macro)
            return DueWithFunctionResult{function, false, ""};

        // 有宏但没有 source → 没有数据来源，原样返回
        if (!source.has_value())
            return DueWithFunctionResult{function, true, "Function has macros but no source provided."};

        // 用 source 在 storage 里查 NBT
        auto it = storage.data.find(*source);
        if (it == storage.data.end())
            return DueWithFunctionResult{function, true, "Source not found."}; // 找不到就原样返回

        NBT &root = it->second;

        // 构建宏表
        std::map<std::string, std::string> macros;

        if (path.has_value())
        {
            // 有 path：求值，取第一个命中的 Compound
            auto hits = NbtPathEvaluator::evaluate(root, *path);
            for (const NBT *hit : hits)
            {
                if (hit->is_compound())
                {
                    collect_macros(hit->as_compound(), macros);
                    break;
                }
            }
        }
        else
        {
            // 没 path：直接把 root 当 Compound
            if (root.is_compound())
            {
                collect_macros(root.as_compound(), macros);
            }
        }

        // 第二步：逐行处理，拼出结果字符串
        std::string result;
        {
            // 记录原始输入是否以换行结尾，最后补回
            bool ends_with_newline = !function.empty() && function.back() == '\n';

            std::istringstream stream(function);
            std::string line;
            bool first = true;
            while (std::getline(stream, line))
            {
                if (!first)
                    result += '\n';
                first = false;

                if (!line.empty() && line[0] == '$')
                    result += expand_line(line.substr(1), macros);
                else
                    result += line;
            }

            if (ends_with_newline && !result.empty() && result.back() != '\n')
                result += '\n';
        }

        return DueWithFunctionResult{result, false, ""};
    }

    ReturnState executeFunction(const Function &function, const std::optional<std::string> &source, const std::optional<NbtPath> &path)
    {
        auto due_with_function_result = dueWithFunction(function.content, source, path);
        if (due_with_function_result.hasError)
        {
            return ReturnState{-1, false, false, "", due_with_function_result.errorMessage, "", ""};
        }
        std::string processed_function = due_with_function_result.processedFunction;
#ifdef _DEBUG
        std::cout << "Executing function:\n"
                  << processed_function << std::endl;
#endif
        std::string output, error, warn, info;
        std::string_view processed_function_view(processed_function);
        TokenizeContext context = initTokenizeContext(processed_function_view);
        Token token{};
        while (true)
        {
#ifdef _DEBUG
            std::cout << "Tokenizing " << token.tokentype << std::endl;
#endif
            if (token.tokentype == TOK_END)
                return ReturnState{0, false, false, output, error, warn, info};
            tokenize(context, token);
            if (token.tokentype == TOK_NEWLINE)
                continue;
            auto it = commandToFunction.find(token.string);
            if (it != commandToFunction.end() && it->second != nullptr)
            {
                auto ret = it->second(token, context);
                warn = ret.warn = warn + ret.warn;
                info = ret.info = info + ret.info;
                error = ret.error = error + ret.error;
                output = ret.out = output + ret.out;
                if (ret.isReturn)
                {
                    return ret;
                };
            }
            else
            {
                warn += "Unknown command: " + std::string(token.string) + ",position: " + std::to_string(token.start.first) + ":" + std::to_string(token.start.second) + "\n";
                while (token.tokentype != TOK_NEWLINE && token.tokentype != TOK_END)
                    tokenize(context, token);
            }
        }
    }
    namespace commandRunner
    {
        ReturnState runSay(Token &token, TokenizeContext &context)
        {
            auto start = token.start;
            tokenize(context, token);

            ReturnState ret;
            if (token.tokentype != TOK_WORD && token.tokentype != TOK_STRING)
            {
                while (token.tokentype != TOK_NEWLINE && token.tokentype != TOK_END)
                    tokenize(context, token);
                return ReturnState{-1, false, false, "", "say command requires a message argument", "", ""};
            }
            else if (token.tokentype == TOK_STRING)
            {
                token.string.remove_prefix(1);
                token.string.remove_suffix(1);
                std::string message = std::string(token.string); // 去掉引号
                ret = ReturnState{0, true, false, message + "\n", "", "", "output(" + std::to_string(start.first) + ":" + std::to_string(start.second) + "): " + message + "\n"};
            }
            else
            {
                std::string message = std::string(token.string);
                ret = ReturnState{0, true, false, message + "\n", "", "", "output(" + std::to_string(start.first) + ":" + std::to_string(start.second) + "): " + message + "\n"};
            }
            tokenize(context, token);
            if (token.tokentype != TOK_NEWLINE && token.tokentype != TOK_END)
            {
                while (token.tokentype != TOK_NEWLINE && token.tokentype != TOK_END)
                    tokenize(context, token);
                ret.error = "say command only takes one argument";
                ret.successFlag = false;
            }
            return ret;
        }
        ReturnState runReturn(Token &token, TokenizeContext &context)
        {
            auto start = token.start;
            tokenize(context, token);
            ReturnState ret;
            if (isPureInteger(token.string))
            {
                int returnValue = std::stoi(std::string(token.string));
                ret = ReturnState{returnValue, true, true, "", "", "", "return(" + std::to_string(start.first) + ":" + std::to_string(start.second) + "): " + std::to_string(returnValue) + "\n"};
            }
            else
            {
                ret = ReturnState{-1, false, true, "", "return command requires an integer argument", "", ""};
            }
            tokenize(context, token);
            if (token.tokentype != TOK_NEWLINE && token.tokentype != TOK_END)
            {
                while (token.tokentype != TOK_NEWLINE && token.tokentype != TOK_END)
                    tokenize(context, token);
                ret.error = "return command only takes one argument";
                ret.successFlag = false;
            }
            return ret;
        }
    } // namespace commandRunner
} // namespace craftsim
