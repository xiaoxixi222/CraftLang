#include <sim/function.hpp>
#include <sim/sim.hpp>
#include <sim/datapack.hpp>
#include <optional>
#include <iostream>
#include <sstream>
#include <map>
#include <charconv>
#include <cstdint>
#include <limits>

namespace craftsim
{
    std::map<std::string, craftsim::CommandFunc, std::less<>> commandToFunction = {
        {"return", commandRunner::runReturn},
        {"scoreboard", commandRunner::runScoreboard},
        {"execute", commandRunner::runExecute},
        {"function", commandRunner::runFunction},
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

        void skipToLineEnd(Token &token, TokenizeContext &context)
        {
            while (token.tokentype != TOK_NEWLINE && token.tokentype != TOK_END)
                tokenize(context, token);
        }

        std::optional<std::string> nextWord(Token &token, TokenizeContext &context)
        {
            if (token.tokentype == TOK_NEWLINE || token.tokentype == TOK_END)
                return std::nullopt;
            tokenize(context, token);
            if (token.tokentype != TOK_WORD)
                return std::nullopt;
            return std::string(token.string);
        }

        bool finishLine(Token &token, TokenizeContext &context)
        {
            if (token.tokentype == TOK_NEWLINE || token.tokentype == TOK_END)
                return true;
            tokenize(context, token);
            if (token.tokentype == TOK_NEWLINE || token.tokentype == TOK_END)
                return true;
            skipToLineEnd(token, context);
            return false;
        }

        ReturnState successResult()
        {
            return ReturnState{0, true, false, "", {}, {}, {}};
        }

        ReturnState errorResult(const std::pair<int, int> &position, std::string message)
        {
            return ReturnState{-1, false, false, "", {{position, std::move(message)}}, {}, {}};
        }

        void addError(ReturnState &state, const std::pair<int, int> &position, std::string message)
        {
            state.error.push_back({position, std::move(message)});
            state.successFlag = false;
        }

        void addWarning(ReturnState &state, const std::pair<int, int> &position, std::string message)
        {
            state.warn.push_back({position, std::move(message)});
        }

        ReturnState parseFailure(const std::pair<int, int> &start, Token &token, TokenizeContext &context, std::string message)
        {
            skipToLineEnd(token, context);
            return errorResult(start, std::move(message));
        }

        std::optional<int> parseInteger(std::string_view text)
        {
            if (text.empty())
                return std::nullopt;
            if (text.front() == '+')
                text.remove_prefix(1);
            if (text.empty())
                return std::nullopt;
            int value = 0;
            const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
            if (result.ec != std::errc{} || result.ptr != text.data() + text.size())
                return std::nullopt;
            return value;
        }

        struct IntRange
        {
            std::optional<int> minimum;
            std::optional<int> maximum;
        };

        std::optional<IntRange> parseRange(std::string_view text)
        {
            const size_t separator = text.find("..");
            if (separator == std::string_view::npos)
            {
                const auto value = parseInteger(text);
                if (!value.has_value())
                    return std::nullopt;
                return IntRange{value, value};
            }
            if (text.find("..", separator + 2) != std::string_view::npos)
                return std::nullopt;
            const std::string_view lower = text.substr(0, separator);
            const std::string_view upper = text.substr(separator + 2);
            if (lower.empty() && upper.empty())
                return std::nullopt;
            IntRange range;
            if (!lower.empty())
            {
                const auto value = parseInteger(lower);
                if (!value.has_value())
                    return std::nullopt;
                range.minimum = value;
            }
            if (!upper.empty())
            {
                const auto value = parseInteger(upper);
                if (!value.has_value())
                    return std::nullopt;
                range.maximum = value;
            }
            return range;
        }

        bool rangeMatches(int value, const IntRange &range)
        {
            if (range.minimum.has_value() && value < *range.minimum)
                return false;
            if (range.maximum.has_value() && value > *range.maximum)
                return false;
            return true;
        }

        bool isComparisonOperator(std::string_view comparison)
        {
            return comparison == "=" || comparison == ">" || comparison == ">=" ||
                   comparison == "<" || comparison == "<=";
        }

        bool compareScores(int left, int right, std::string_view comparison)
        {
            if (comparison == "=")
                return left == right;
            if (comparison == ">")
                return left > right;
            if (comparison == ">=")
                return left >= right;
            if (comparison == "<")
                return left < right;
            if (comparison == "<=")
                return left <= right;
            return false;
        }

        struct ScoreLookup
        {
            bool objectiveExists;
            bool scoreExists;
            int value;
        };

        ScoreLookup lookupScore(const std::string &player, const std::string &objective)
        {
            const auto objectiveIterator = scoreboard.objectives.find(objective);
            if (objectiveIterator == scoreboard.objectives.end())
                return {false, false, 0};
            const auto scoreIterator = objectiveIterator->second.scores.find(player);
            if (scoreIterator == objectiveIterator->second.scores.end())
                return {true, false, 0};
            return {true, true, scoreIterator->second};
        }

        int narrowToInt(int64_t value)
        {
            if (value > std::numeric_limits<int>::max())
                return std::numeric_limits<int>::max();
            if (value < std::numeric_limits<int>::min())
                return std::numeric_limits<int>::min();
            return static_cast<int>(value);
        }

        bool writeStorageInt(const std::string &name, const std::string &key, int value)
        {
            auto iterator = storage.data.find(name);
            if (iterator == storage.data.end())
                iterator = storage.data.emplace(name, NBT::make_compound()).first;
            if (!iterator->second.is_compound())
                return false;
            iterator->second.as_compound()[key] = NBT::make_int(value);
            return true;
        }

        void appendState(ReturnState &destination, const ReturnState &source)
        {
            destination.out += source.out;
            destination.error.insert(destination.error.end(), source.error.begin(), source.error.end());
            destination.warn.insert(destination.warn.end(), source.warn.begin(), source.warn.end());
            destination.info.insert(destination.info.end(), source.info.begin(), source.info.end());
        }

        ReturnState dispatchCommand(Token &token, TokenizeContext &context)
        {
            if (token.tokentype != TOK_WORD)
                return errorResult(token.start, "expected a command");
            const auto iterator = commandToFunction.find(token.string);
            if (iterator == commandToFunction.end() || iterator->second == nullptr)
            {
                const std::pair<int, int> position = token.start;
                std::string command(token.string);
                skipToLineEnd(token, context);
                return errorResult(position, "Unknown command: " + command);
            }
            return iterator->second(token, context);
        }

        ReturnState runNestedCommand(Token &token, TokenizeContext &context)
        {
            const std::pair<int, int> position = token.start;
            tokenize(context, token);
            if (token.tokentype != TOK_WORD)
            {
                skipToLineEnd(token, context);
                return errorResult(position, "execute requires a command after run");
            }
            return dispatchCommand(token, context);
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
        tokenize(context, token, TOKENIZE_NORMAL);
    }
    void tokenize(TokenizeContext &context, Token &token, TokenizeType type)
    {
        switch (type)
        {
        case TOKENIZE_NORMAL:
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
                        if (!start.has_value())
                        {
                            token.tokentype = TOK_ERROR;
                            return;
                        }
                        end = context.pos;
                        context.pos.second++;
                        const auto startPos = start.value();
                        token.start = startPos;
                        token.end = end.value();
                        token.tokentype = TOK_STRING;
                        int startLocation = context.rowStarts[startPos.first] + startPos.second;
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
        case TOKENIZE_LINE:
        {
            std::string_view &function = context.function;
            size_t location = static_cast<size_t>(context.rowStarts[context.pos.first]) +
                              static_cast<size_t>(context.pos.second);
            if (location >= function.size())
            {
                token.start = context.pos;
                token.end = context.pos;
                token.string = "";
                token.tokentype = TOK_END;
                return;
            }

            // 跳过行首空格
            while (location < function.size() && function[location] == ' ')
            {
                ++location;
                ++context.pos.second;
            }
            if (location >= function.size())
            {
                token.start = context.pos;
                token.end = context.pos;
                token.string = "";
                token.tokentype = TOK_END;
                return;
            }

            size_t endLocation = function.find_first_of("\r\n", location);
            if (endLocation == std::string_view::npos)
                endLocation = function.size();

            size_t length = endLocation - location;

            token.start = context.pos;
            if (length == 0)
                token.end = context.pos;
            else
                token.end = std::pair<int, int>(context.pos.first,
                                                context.pos.second + static_cast<int>(length) - 1);
            token.string = function.substr(location, length);
            token.tokentype = TOK_WORD;

            // 只移动到行尾，换行符留给 TOKENIZE_NORMAL 产生 TOK_NEWLINE
            context.pos.second += static_cast<int>(length);
            return;
        }
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
            return ReturnState{-1, false, false, {}, {{{-1, -1}, due_with_function_result.errorMessage}}, {}, {}};
        }
        std::string processed_function = due_with_function_result.processedFunction;
#ifdef _DEBUG
        std::cout << "Executing function:\n"
                  << processed_function << std::endl;
#endif
        std::vector<LogEntry> error, warn, info;
        std::string output;
        std::string_view processed_function_view(processed_function);
        TokenizeContext context = initTokenizeContext(processed_function_view);
        Token token{};
        // 先取第一个 token；之后每个 handler 结束时 token 都必须停在
        // NEWLINE 或 END 上。
        tokenize(context, token);
        while (true)
        {
#ifdef _DEBUG
            std::cout << "Tokenizing " << token.tokentype << std::endl;
#endif
            // 先检查再 tokenize：handler 可能已经把 token 留在 END 上
            // （最后一行没有换行时），这里绝不能再 tokenize 一次。
            if (token.tokentype == TOK_END)
                return ReturnState{0, false, false, output, error, warn, info};
            if (token.tokentype == TOK_NEWLINE)
            {
                tokenize(context, token);
                continue;
            }
            const auto ret = dispatchCommand(token, context);
            warn.insert(warn.end(), ret.warn.begin(), ret.warn.end());
            error.insert(error.end(), ret.error.begin(), ret.error.end());
            output += ret.out;
            info.insert(info.end(), ret.info.begin(), ret.info.end());
            if (ret.isReturn)
                return {ret.returnValue, ret.successFlag, false, output, error, warn, info};
        }
    }
    namespace
    {
        ReturnState runScoreboardObjectives(Token &token, TokenizeContext &context, const std::pair<int, int> &start)
        {
            const auto action = nextWord(token, context);
            if (!action.has_value())
                return parseFailure(start, token, context, "scoreboard objectives requires an action");

            if (*action == "add")
            {
                const auto objectiveName = nextWord(token, context);
                if (!objectiveName.has_value())
                    return parseFailure(start, token, context, "scoreboard objectives add requires an objective name");
                const auto objectiveType = nextWord(token, context);
                if (!objectiveType.has_value())
                    return parseFailure(start, token, context, "scoreboard objectives add requires an objective type");
                if (*objectiveType != "dummy")
                    return parseFailure(start, token, context, "scoreboard objectives add only supports dummy");
                if (!finishLine(token, context))
                    return errorResult(start, "scoreboard objectives add takes exactly three arguments");
                if (scoreboard.objectives.find(*objectiveName) != scoreboard.objectives.end())
                    return errorResult(start, "scoreboard objective already exists: " + *objectiveName);
                scoreboard.objectives.emplace(*objectiveName, ScoreProgram{*objectiveName, ScoreType::DUMMY, {}});
                return successResult();
            }

            if (*action == "remove")
            {
                const auto objectiveName = nextWord(token, context);
                if (!objectiveName.has_value())
                    return parseFailure(start, token, context, "scoreboard objectives remove requires an objective name");
                if (!finishLine(token, context))
                    return errorResult(start, "scoreboard objectives remove takes exactly one argument");
                scoreboard.objectives.erase(*objectiveName);
                return successResult();
            }

            return parseFailure(start, token, context, "Unknown scoreboard objectives action: " + *action);
        }
        ReturnState runScoreboardPlayers(Token &token, TokenizeContext &context, const std::pair<int, int> &start)
        {
            const auto action = nextWord(token, context);
            if (!action.has_value())
                return parseFailure(start, token, context, "scoreboard players requires an action");

            if (*action == "get")
            {
                const auto player = nextWord(token, context);
                if (!player.has_value())
                    return parseFailure(start, token, context, "scoreboard players get requires a player");
                const auto objectiveName = nextWord(token, context);
                if (!objectiveName.has_value())
                    return parseFailure(start, token, context, "scoreboard players get requires an objective");
                if (!finishLine(token, context))
                    return errorResult(start, "scoreboard players get takes exactly two arguments");

                const ScoreLookup lookup = lookupScore(*player, *objectiveName);
                ReturnState result = successResult();
                if (!lookup.objectiveExists)
                {
                    addWarning(result, start, "scoreboard objective not found: " + *objectiveName);
                    result.successFlag = false;
                    return result;
                }
                if (!lookup.scoreExists)
                    addWarning(result, start, "score not set for " + *player + " in " + *objectiveName + ", using 0");
                result.returnValue = lookup.value;
                return result;
            }

            if (*action == "set" || *action == "add" || *action == "remove")
            {
                const auto player = nextWord(token, context);
                if (!player.has_value())
                    return parseFailure(start, token, context, "scoreboard players " + *action + " requires a player");
                const auto objectiveName = nextWord(token, context);
                if (!objectiveName.has_value())
                    return parseFailure(start, token, context, "scoreboard players " + *action + " requires an objective");
                const auto valueText = nextWord(token, context);
                if (!valueText.has_value())
                    return parseFailure(start, token, context, "scoreboard players " + *action + " requires an integer");
                if (!finishLine(token, context))
                    return errorResult(start, "scoreboard players " + *action + " takes exactly three arguments");
                const auto value = parseInteger(*valueText);
                if (!value.has_value())
                    return errorResult(start, "invalid integer: " + *valueText);

                const ScoreLookup lookup = lookupScore(*player, *objectiveName);
                ReturnState result = successResult();
                if (!lookup.objectiveExists)
                {
                    addWarning(result, start, "scoreboard objective not found: " + *objectiveName);
                    result.successFlag = false;
                    return result;
                }
                if (!lookup.scoreExists && *action != "set")
                    addWarning(result, start, "score not set for " + *player + " in " + *objectiveName + ", using 0");

                int nextValue = *value;
                if (*action == "add")
                    nextValue = narrowToInt(static_cast<int64_t>(lookup.value) + *value);
                else if (*action == "remove")
                    nextValue = narrowToInt(static_cast<int64_t>(lookup.value) - *value);
                scoreboard.objectives.at(*objectiveName).scores[*player] = nextValue;
                return result;
            }

            if (*action == "operation")
            {
                const auto targetPlayer = nextWord(token, context);
                if (!targetPlayer.has_value())
                    return parseFailure(start, token, context, "scoreboard players operation requires a target player");
                const auto targetObjective = nextWord(token, context);
                if (!targetObjective.has_value())
                    return parseFailure(start, token, context, "scoreboard players operation requires a target objective");
                const auto operation = nextWord(token, context);
                if (!operation.has_value())
                    return parseFailure(start, token, context, "scoreboard players operation requires an operator");
                const auto sourcePlayer = nextWord(token, context);
                if (!sourcePlayer.has_value())
                    return parseFailure(start, token, context, "scoreboard players operation requires a source player");
                const auto sourceObjective = nextWord(token, context);
                if (!sourceObjective.has_value())
                    return parseFailure(start, token, context, "scoreboard players operation requires a source objective");
                if (!finishLine(token, context))
                    return errorResult(start, "scoreboard players operation takes exactly five arguments");

                const bool validOperator = *operation == "=" || *operation == "+=" || *operation == "-=" ||
                                           *operation == "*=" || *operation == "/=" || *operation == "%=";
                if (!validOperator)
                    return errorResult(start, "unknown scoreboard operator: " + *operation);

                const ScoreLookup target = lookupScore(*targetPlayer, *targetObjective);
                const ScoreLookup source = lookupScore(*sourcePlayer, *sourceObjective);
                ReturnState result = successResult();
                if (!target.objectiveExists)
                {
                    addWarning(result, start, "scoreboard objective not found: " + *targetObjective);
                    result.successFlag = false;
                    return result;
                }
                if (!source.objectiveExists)
                {
                    addWarning(result, start, "scoreboard objective not found: " + *sourceObjective);
                    result.successFlag = false;
                    return result;
                }
                if (!source.scoreExists)
                    addWarning(result, start, "score not set for " + *sourcePlayer + " in " + *sourceObjective + ", using 0");
                if (!target.scoreExists && *operation != "=")
                    addWarning(result, start, "score not set for " + *targetPlayer + " in " + *targetObjective + ", using 0");

                int64_t nextValue = target.value;
                const int64_t sourceValue = source.value;
                if (*operation == "=")
                    nextValue = sourceValue;
                else if (*operation == "+=")
                    nextValue += sourceValue;
                else if (*operation == "-=")
                    nextValue -= sourceValue;
                else if (*operation == "*=")
                    nextValue *= sourceValue;
                else if (*operation == "/=")
                {
                    if (sourceValue == 0)
                        return errorResult(start, "scoreboard players operation: division by zero");
                    nextValue /= sourceValue;
                }
                else
                {
                    if (sourceValue == 0)
                        return errorResult(start, "scoreboard players operation: modulo by zero");
                    nextValue %= sourceValue;
                }
                scoreboard.objectives.at(*targetObjective).scores[*targetPlayer] = narrowToInt(nextValue);
                return result;
            }

            return parseFailure(start, token, context, "Unknown scoreboard players action: " + *action);
        }
        ReturnState runExecuteCondition(Token &token, TokenizeContext &context, const std::pair<int, int> &start, bool negate)
        {
            const auto conditionType = nextWord(token, context);
            if (!conditionType.has_value())
                return parseFailure(start, token, context, "execute if/unless requires a condition");
            if (*conditionType != "score")
                return parseFailure(start, token, context, "execute only supports score conditions");

            const auto leftPlayer = nextWord(token, context);
            if (!leftPlayer.has_value())
                return parseFailure(start, token, context, "execute if score requires a player");
            const auto leftObjective = nextWord(token, context);
            if (!leftObjective.has_value())
                return parseFailure(start, token, context, "execute if score requires an objective");
            const auto comparison = nextWord(token, context);
            if (!comparison.has_value())
                return parseFailure(start, token, context, "execute if score requires a comparison");

            bool useRange = false;
            IntRange range{};
            std::string rangeText;
            std::string rightPlayer;
            std::string rightObjective;
            if (*comparison == "matches")
            {
                const auto rangeValue = nextWord(token, context);
                if (!rangeValue.has_value())
                    return parseFailure(start, token, context, "execute if score matches requires a range");
                const auto parsedRange = parseRange(*rangeValue);
                if (!parsedRange.has_value())
                    return parseFailure(start, token, context, "invalid range: " + *rangeValue);
                useRange = true;
                range = *parsedRange;
                rangeText = *rangeValue;
            }
            else
            {
                if (!isComparisonOperator(*comparison))
                    return parseFailure(start, token, context, "invalid comparison: " + *comparison);
                const auto parsedRightPlayer = nextWord(token, context);
                if (!parsedRightPlayer.has_value())
                    return parseFailure(start, token, context, "execute if score requires a second player");
                const auto parsedRightObjective = nextWord(token, context);
                if (!parsedRightObjective.has_value())
                    return parseFailure(start, token, context, "execute if score requires a second objective");
                rightPlayer = *parsedRightPlayer;
                rightObjective = *parsedRightObjective;
            }

            const auto runKeyword = nextWord(token, context);
            if (!runKeyword.has_value() || *runKeyword != "run")
                return parseFailure(start, token, context, "execute if score requires run");

            ReturnState result = successResult();
            const ScoreLookup left = lookupScore(*leftPlayer, *leftObjective);
            if (!left.objectiveExists)
                addWarning(result, start, "execute objective not found: " + *leftObjective);
            else if (!left.scoreExists)
                addWarning(result, start, "score not set for " + *leftPlayer + " in " + *leftObjective + ", using 0");

            int rightValue = 0;
            bool rightObjectiveExists = true;
            if (!useRange)
            {
                const ScoreLookup right = lookupScore(rightPlayer, rightObjective);
                rightValue = right.value;
                rightObjectiveExists = right.objectiveExists;
                if (!right.objectiveExists)
                    addWarning(result, start, "execute objective not found: " + rightObjective);
                else if (!right.scoreExists)
                    addWarning(result, start, "score not set for " + rightPlayer + " in " + rightObjective + ", using 0");
            }

            bool condition = false;
            if (useRange)
                condition = left.objectiveExists && rangeMatches(left.value, range);
            else if (left.objectiveExists && rightObjectiveExists)
                condition = compareScores(left.value, rightValue, *comparison);
            if (negate)
                condition = !condition;

            std::string description = "score " + *leftPlayer + " " + *leftObjective;
            if (useRange)
                description += " matches " + rangeText;
            else
                description += " " + *comparison + " " + rightPlayer + " " + rightObjective;
            result.info.push_back({start, std::string(negate ? "unless " : "if ") + description + " -> " + (condition ? "true" : "false")});

            if (!condition)
            {
                skipToLineEnd(token, context);
                return result;
            }

            const ReturnState nested = runNestedCommand(token, context);
            appendState(result, nested);
            if (nested.isReturn)
            {
                result.isReturn = true;
                result.returnValue = nested.returnValue;
                result.successFlag = nested.successFlag;
            }
            return result;
        }
        ReturnState runExecuteStore(Token &token, TokenizeContext &context, const std::pair<int, int> &start)
        {
            const auto category = nextWord(token, context);
            if (!category.has_value())
                return parseFailure(start, token, context, "execute store requires result");
            if (*category != "result")
                return parseFailure(start, token, context, "execute store only supports result");
            const auto target = nextWord(token, context);
            if (!target.has_value())
                return parseFailure(start, token, context, "execute store result requires a target");
            if (*target == "score")
            {
                skipToLineEnd(token, context);
                return errorResult(start, "execute store result score is not supported; use storage");
            }
            if (*target != "storage")
                return parseFailure(start, token, context, "execute store result only supports storage");
            const auto storageName = nextWord(token, context);
            if (!storageName.has_value())
                return parseFailure(start, token, context, "execute store result storage requires a namespace");
            const auto key = nextWord(token, context);
            if (!key.has_value())
                return parseFailure(start, token, context, "execute store result storage requires a key");
            if (key->find_first_of(".[]{}") != std::string::npos)
                return parseFailure(start, token, context, "execute store result storage only supports a single key");
            const auto type = nextWord(token, context);
            if (!type.has_value())
                return parseFailure(start, token, context, "execute store result storage requires a type");
            if (*type != "int")
                return parseFailure(start, token, context, "execute store result storage only supports int");
            const auto scaleText = nextWord(token, context);
            if (!scaleText.has_value())
                return parseFailure(start, token, context, "execute store result storage requires a scale");
            const auto scale = parseInteger(*scaleText);
            if (!scale.has_value())
                return parseFailure(start, token, context, "invalid scale: " + *scaleText);
            if (*scale == 0)
                return parseFailure(start, token, context, "execute store scale must not be zero");
            const auto runKeyword = nextWord(token, context);
            if (!runKeyword.has_value() || *runKeyword != "run")
                return parseFailure(start, token, context, "execute store requires run");

            const ReturnState nested = runNestedCommand(token, context);
            ReturnState result = successResult();
            appendState(result, nested);
            const int sourceValue = nested.successFlag ? nested.returnValue : 0;
            const int storedValue = narrowToInt(static_cast<int64_t>(sourceValue) / *scale);
            if (!writeStorageInt(*storageName, *key, storedValue))
                addError(result, start, "storage " + *storageName + " is not a compound");
            else
                result.info.push_back({start, "store result storage " + *storageName + " " + *key + " = " + std::to_string(storedValue)});
            if (nested.isReturn)
            {
                result.isReturn = true;
                result.returnValue = nested.returnValue;
                result.successFlag = nested.successFlag;
            }
            return result;
        }
    } // namespace
    namespace commandRunner
    {
        ReturnState runSay(Token &token, TokenizeContext &context)
        {
            auto start = token.start;
            tokenize(context, token, TOKENIZE_LINE);

            if (token.tokentype == TOK_END)
                return ReturnState{-1, false, false, {}, {{start, "say command requires a message argument"}}, {}, {}};

            std::string message(token.string);

            // 行模式停在 \r/\n 前；补一次普通 tokenize，
            // 让 handler 结束时 token 一定落在 NEWLINE 或 END 上。
            // 如果这一行就是文件末尾，这里会拿到 END；
            // executeFunction 会先检查 END 再决定是否继续，不会重复 tokenize。
            tokenize(context, token);

            if (message.empty())
                return ReturnState{-1, false, false, {}, {{start, "say command requires a message argument"}}, {}, {}};

            return ReturnState{0, true, false, message + "\n", {}, {}, {{start, "say:" + message}}};
        }
        ReturnState runReturn(Token &token, TokenizeContext &context)
        {
            auto start = token.start;
            tokenize(context, token);
            ReturnState ret;
            if (token.tokentype == TOK_WORD && isPureInteger(token.string))
            {
                int returnValue = std::stoi(std::string(token.string));
                ret = ReturnState{returnValue, true, true, "", {}, {}, {{start, "return: " + std::to_string(returnValue)}}};
                tokenize(context, token);
                if (token.tokentype != TOK_NEWLINE && token.tokentype != TOK_END)
                {
                    while (token.tokentype != TOK_NEWLINE && token.tokentype != TOK_END)
                        tokenize(context, token);
                    ret.error.push_back({start, "return command only takes one argument"});
                    ret.successFlag = false;
                }
            }
            else
            {
                // 缺参数时 token 可能已经是行尾，不能再 tokenize，否则会吞掉下一行
                while (token.tokentype != TOK_NEWLINE && token.tokentype != TOK_END)
                    tokenize(context, token);
                ret = ReturnState{-1, false, true, "", {{start, "return command requires an integer argument"}}, {}, {}};
            }
            return ret;
        }
        ReturnState runScoreboard(Token &token, TokenizeContext &context)
        {
            const auto start = token.start;
            const auto subcommand = nextWord(token, context);
            if (!subcommand.has_value())
                return parseFailure(start, token, context, "scoreboard requires a subcommand");
            if (*subcommand == "objectives")
                return runScoreboardObjectives(token, context, start);
            if (*subcommand == "players")
                return runScoreboardPlayers(token, context, start);
            return parseFailure(start, token, context, "Unknown scoreboard subcommand: " + *subcommand);
        }
        ReturnState runExecute(Token &token, TokenizeContext &context)
        {
            const auto start = token.start;
            const auto subcommand = nextWord(token, context);
            if (!subcommand.has_value())
                return parseFailure(start, token, context, "execute requires a subcommand");
            if (*subcommand == "run")
                return runNestedCommand(token, context);
            if (*subcommand == "if")
                return runExecuteCondition(token, context, start, false);
            if (*subcommand == "unless")
                return runExecuteCondition(token, context, start, true);
            if (*subcommand == "store")
                return runExecuteStore(token, context, start);
            return parseFailure(start, token, context, "Unsupported execute subcommand: " + *subcommand);
        }
        ReturnState runFunction(Token &token, TokenizeContext &context)
        {
            const auto start = token.start;
            const auto fullName = nextWord(token, context);
            if (!fullName.has_value())
                return parseFailure(start, token, context, "function requires <namespace>:<path>");
            const size_t separator = fullName->find(':');
            if (separator == std::string::npos || separator == 0 || separator + 1 >= fullName->size())
                return parseFailure(start, token, context, "function name must be <namespace>:<path>");

            const std::string namespaceName = fullName->substr(0, separator);
            const std::string functionName = fullName->substr(separator + 1);
            std::optional<std::string> source;
            std::optional<NbtPath> path;

            const auto modifier = nextWord(token, context);
            if (modifier.has_value())
            {
                if (*modifier != "with")
                    return parseFailure(start, token, context, "function only supports with storage");
                const auto storageKeyword = nextWord(token, context);
                if (!storageKeyword.has_value() || *storageKeyword != "storage")
                    return parseFailure(start, token, context, "function with requires storage");
                const auto storageName = nextWord(token, context);
                if (!storageName.has_value())
                    return parseFailure(start, token, context, "function with storage requires a namespace");
                source = *storageName;
                const auto pathName = nextWord(token, context);
                if (pathName.has_value())
                {
                    if (pathName->find_first_of(".[]{}") != std::string::npos)
                        return parseFailure(start, token, context, "function storage path only supports a single key");
                    NbtPath nbtPath;
                    nbtPath.segments.push_back(PathNode{PathNode::Key{*pathName}});
                    path = std::move(nbtPath);
                }
            }

            if (!finishLine(token, context))
                return errorResult(start, "function takes too many arguments");

            const auto namespaceIterator = function_map.find(namespaceName);
            if (namespaceIterator == function_map.end())
                return errorResult(start, "function not found: " + *fullName);
            const auto functionIterator = namespaceIterator->second.find(functionName);
            if (functionIterator == namespaceIterator->second.end())
                return errorResult(start, "function not found: " + *fullName);

            ReturnState child = executeFunction(functionIterator->second, source, path);
            for (auto &entry : child.error)
            {
                if (entry.position.first == -1)
                    entry.position = start;
            }
            ReturnState result{child.returnValue, true, false, "", {}, {}, {}};
            appendState(result, child);
            return result;
        }
    } // namespace commandRunner
} // namespace craftsim
