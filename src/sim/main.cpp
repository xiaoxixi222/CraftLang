#include <json.hpp>
#include <iostream>
#include <sim/function.hpp>
#include <sim/storage.hpp>
#include <sim/datapack.hpp>
#include <sim/sim.hpp>

craftsim::Storage storage;
craftsim::Scoreboard scoreboard;

int main()
{
    craftsim::loadDatapack("example/test");
    auto return_state = craftsim::executeFunction(craftsim::function_map["test"]["test"], std::nullopt, std::nullopt);
    std::cout << "Return value: " << return_state.returnValue << std::endl;
    std::cout << return_state.out << std::endl;
    for (const auto &entry : return_state.error)
        std::cout << "Error: (" << entry.position.first << ":" << entry.position.second << ") " << entry.message << std::endl;
    for (const auto &entry : return_state.warn)
        std::cout << "Warn: (" << entry.position.first << ":" << entry.position.second << ") " << entry.message << std::endl;
    for (const auto &entry : return_state.info)
        std::cout << "Info: (" << entry.position.first << ":" << entry.position.second << ") " << entry.message << std::endl;
    return 0;
}
