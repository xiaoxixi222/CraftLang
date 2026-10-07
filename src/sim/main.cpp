#include <json.hpp>
#include <sim/tokenize.hpp>

int main()
{
    std::string testFunction = R"(scoreboard objectives remove return
scoreboard objectives add return dummy
scoreboard players operation ..name.. return = ..name.. 1_tmp_0
scoreboard players set ..name.. interrupted 2
return 0
data get storage craftlang "123 1234" 2)";
    std::string_view function(testFunction);
    craftsim::Token token;
    craftsim::TokenizeContext context;
    context = craftsim::initTokenizeContext(function);
    printf("%s\n", testFunction.c_str());
    while(true)
    {
        craftsim::tokenize(context, token);
        printf("token %u from (%d, %d) to (%d, %d), %s\n",
               token.tokentype,
               token.start.first,
               token.start.second,
               token.end.first,
               token.end.second,
               std::string(token.string).c_str());
        if (token.tokentype == craftsim::TOK_END)
        {
            break;
        }
    }
    return 0;
}
