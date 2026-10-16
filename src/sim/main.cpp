#include <json.hpp>
#include <sim/function.hpp>
#include <sim/storage.hpp>
#include <sim/datapack.hpp>
#include <sim/sim.hpp>

craftsim::Storage storage;
craftsim::Scoreboard scoreboard;

int main()
{
    craftsim::loadDatapack("example/test");
    craftsim::executeFunction(craftsim::function_map["test"]["test"], std::nullopt, std::nullopt);
    return 0;
}
