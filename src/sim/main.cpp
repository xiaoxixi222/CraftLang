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
    std::cout << "Return value: " << return_state.returnValue <<std::endl
              << "output: " << return_state.out << std::endl
              << "error: " << return_state.error << std::endl
              << "warn: " << return_state.warn << std::endl
              << "info: " << return_state.info << std::endl;
    return 0;
}
