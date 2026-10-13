#include <nanobench.h>

#include <memory>
#include <string>
#include <vector>
#include "Benchmarks.h"
#include "Core/Components/TransformComponent.h"
#include "Core/Entity.h"
#include "Fakes/FakeNetworkService.h"
#include "Services/MessageService.h"
#include "Services/ScriptService.h"

using namespace Elysium;
using ankerl::nanobench::Bench;

namespace {

constexpr size_t Count = 1000;

// The real ScriptService with every binding a game script sees, pointed at a World of
// `Count` entities that each have a Transform. `entities` is that list as a Lua table.
struct ScriptScenario {
    ServiceLocator locator;
    Services::ScriptService& scripts;
    World world;

    ScriptScenario()
        : scripts(locator.Register<Services::ScriptService, Services::IScriptService>(
              std::make_unique<Services::ScriptService>(locator))) {
        locator.Register<Services::MessageService, Services::IMessageService>(
            std::make_unique<Services::MessageService>(locator));
        locator.Register<Tests::FakeNetworkService, Services::INetworkService>(
            std::make_unique<Tests::FakeNetworkService>());
        scripts.Initialize();
        scripts.SetActiveWorld(&world);

        sol::table entities = scripts.GetLua().create_table();
        for (size_t i = 0; i < Count; ++i) {
            Entity e = world.CreateEntity();
            world.AddComponent(e, TransformComponent((float)i, (float)i));
            entities[i + 1] = e;
        }
        scripts.GetLua()["entities"] = entities;
    }

    // Defines `body` as a Lua function looping over every entity and returns it for the
    // benchmark to call; `e` is the entity, `t` a Transform fetched before the loop.
    sol::protected_function Loop(const std::string& body) {
        const std::string source = "return function()\n"
                                   "  local t = GetComponent(entities[1], 'Transform')\n"
                                   "  local sum = 0\n"
                                   "  for i = 1, #entities do\n"
                                   "    local e = entities[i]\n"
                                   "    " + body + "\n"
                                   "  end\n"
                                   "  return sum\n"
                                   "end";
        return scripts.GetLua().script(source).get<sol::protected_function>();
    }
};

void Run(Bench& bench, ScriptScenario& scenario, const char* name, const std::string& body) {
    sol::protected_function loop = scenario.Loop(body);
    bench.run(name, [&] { loop(); });
}

}  // namespace

void RunScriptBenchmarks() {
    ScriptScenario scenario;
    Bench bench;
    bench.title("Lua component access, " + std::to_string(Count) + " entities")
        .unit("access")
        .batch(Count)
        .warmup(3)
        .minEpochIterations(50);

    Run(bench, scenario, "Baseline: Lua loop doing plain arithmetic", "sum = sum + e");
    Run(bench, scenario, "Get a component", "local c = GetComponent(e, 'Transform')");
    Run(bench, scenario, "Get a component and read a field", "sum = sum + GetComponent(e, 'Transform').localX");
    Run(bench, scenario, "Get a component and write a field", "GetComponent(e, 'Transform').localX = 1");
    Run(bench, scenario, "Read a field of a component already fetched", "sum = sum + t.localX");
    Run(bench, scenario, "Write a field of a component already fetched", "t.localX = e");
    Run(bench, scenario, "Set a component from a table", "SetComponent(e, 'Transform', { localX = 1 })");
}
