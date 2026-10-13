#include <nanobench.h>

#include <algorithm>
#include <memory>
#include <random>
#include <string>
#include <vector>
#include "Core/Entity.h"
#include "Benchmarks.h"

using namespace Elysium;
using ankerl::nanobench::Bench;
using ankerl::nanobench::doNotOptimizeAway;

namespace {

struct Position {
    float x = 0.0f, y = 0.0f;
};
struct Velocity {
    float x = 1.0f, y = 1.0f;
};

// `count` entities, all with a Position; every `velocityEvery`-th one also has a Velocity.
struct Scenario {
    std::unique_ptr<World> world = std::make_unique<World>();
    std::vector<Entity> entities;

    Scenario(size_t count, size_t velocityEvery) {
        world->RegisterComponent<Position>();
        world->RegisterComponent<Velocity>();
        for (size_t i = 0; i < count; ++i) {
            Entity e = world->CreateEntity();
            world->AddComponent(e, Position{(float)i, (float)i});
            if (i % velocityEvery == 0) world->AddComponent(e, Velocity{});
            entities.push_back(e);
        }
    }
};

void BenchmarkLifecycle(Bench& bench, size_t count) {
    World world;
    world.RegisterComponent<Position>();
    world.RegisterComponent<Velocity>();
    std::vector<Entity> entities(count);
    bench.batch(count).run("Create an entity, add two components, destroy it", [&] {
        for (size_t i = 0; i < count; ++i) {
            Entity e = world.CreateEntity();
            world.AddComponent(e, Position{});
            world.AddComponent(e, Velocity{});
            entities[i] = e;
        }
        for (Entity e : entities) world.DestroyEntity(e);
    });
}

void BenchmarkAccess(Bench& bench, size_t count) {
    Scenario scenario(count, 1);
    std::vector<Entity> shuffled = scenario.entities;
    std::shuffle(shuffled.begin(), shuffled.end(), std::mt19937(42));

    bench.batch(count).run("Get a component, entities in random order", [&] {
        float sum = 0.0f;
        for (Entity e : shuffled) sum += scenario.world->GetComponent<Position>(e).x;
        doNotOptimizeAway(sum);
    });
    bench.batch(count).run("Check for a component, entities in random order", [&] {
        size_t hits = 0;
        for (Entity e : shuffled) hits += scenario.world->HasComponent<Velocity>(e);
        doNotOptimizeAway(hits);
    });
}

void BenchmarkQueries(Bench& bench, size_t count) {
    std::vector<Position> plain(count);
    bench.batch(count).run("Baseline: plain loop over a vector of components", [&] {
        float sum = 0.0f;
        for (const Position& p : plain) sum += p.x;
        doNotOptimizeAway(sum);
    });

    Scenario all(count, 1);
    bench.batch(count).run("Query one component, every entity has it", [&] {
        float sum = 0.0f;
        all.world->Query<Position>([&](Entity, Position& p) { sum += p.x; });
        doNotOptimizeAway(sum);
    });

    for (size_t every : {2, 10}) {
        Scenario scenario(count, every);
        std::string share = std::to_string(100 / every) + "% of entities have both";
        // Batched per living entity: a query's cost is paid for every entity it scans.
        bench.batch(count).run("Query two components, " + share, [&] {
            scenario.world->Query<Position, Velocity>([](Entity, Position& p, Velocity& v) {
                p.x += v.x;
                p.y += v.y;
            });
        });
        bench.batch(count).run("List entities with two components, " + share, [&] {
            doNotOptimizeAway(scenario.world->GetEntitiesWithComponents<Position, Velocity>().size());
        });
    }
}

}  // namespace

void RunWorldBenchmarks() {
    // MAX_ENTITIES (Core/Entity.h) caps a World at 10000.
    for (size_t count : {size_t(1000), MAX_ENTITIES}) {
        Bench bench;
        bench.title("World, " + std::to_string(count) + " entities").unit("entity").warmup(3).minEpochIterations(50);
        BenchmarkLifecycle(bench, count);
        BenchmarkAccess(bench, count);
        BenchmarkQueries(bench, count);
    }
}
