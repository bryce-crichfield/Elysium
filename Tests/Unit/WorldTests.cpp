#include <doctest/doctest.h>
#include <map>
#include "Core/Entity.h"

using namespace Elysium;

namespace {

struct Position {
    float x = 0.0f;
};
struct Velocity {
    float x = 0.0f;
};

struct TwoComponentWorld {
    World world;
    Entity both, positionOnly, velocityOnly;

    TwoComponentWorld() {
        world.RegisterComponent<Position>();
        world.RegisterComponent<Velocity>();
        both = world.CreateEntity();
        positionOnly = world.CreateEntity();
        velocityOnly = world.CreateEntity();
        world.AddComponent(both, Position{1.0f});
        world.AddComponent(both, Velocity{10.0f});
        world.AddComponent(positionOnly, Position{2.0f});
        world.AddComponent(velocityOnly, Velocity{20.0f});
    }
};

}  // namespace

TEST_CASE("Query visits each entity with all the components, passing its own components") {
    TwoComponentWorld w;

    std::map<Entity, float> positions;
    w.world.Query<Position>([&](Entity e, Position& p) { positions[e] = p.x; });
    CHECK(positions == std::map<Entity, float>{{w.both, 1.0f}, {w.positionOnly, 2.0f}});

    int visits = 0;
    w.world.Query<Position, Velocity>([&](Entity e, Position& p, Velocity& v) {
        ++visits;
        CHECK(e == w.both);
        CHECK(p.x == 1.0f);
        CHECK(v.x == 10.0f);
    });
    CHECK(visits == 1);
}

TEST_CASE("Query hands out references to the stored components") {
    TwoComponentWorld w;
    w.world.Query<Position, Velocity>([](Entity, Position& p, Velocity& v) { p.x += v.x; });
    CHECK(w.world.GetComponent<Position>(w.both).x == 11.0f);
    CHECK(w.world.GetComponent<Position>(w.positionOnly).x == 2.0f);
}

TEST_CASE("Components stay attached to the right entity after another entity's is removed") {
    TwoComponentWorld w;
    w.world.RemoveComponent<Position>(w.both);
    CHECK_FALSE(w.world.HasComponent<Position>(w.both));
    CHECK(w.world.GetComponent<Position>(w.positionOnly).x == 2.0f);
    CHECK_THROWS(w.world.GetComponent<Position>(w.both));
}

TEST_CASE("Using a component type that was never registered throws") {
    struct Unregistered {};
    World world;
    Entity e = world.CreateEntity();
    CHECK_THROWS(world.HasComponent<Unregistered>(e));
    CHECK_THROWS(world.GetComponent<Unregistered>(e));
}

TEST_CASE("Each component type gets one id, the same in every World") {
    CHECK(ComponentId<Position>() == ComponentId<Position>());
    CHECK(ComponentId<Position>() != ComponentId<Velocity>());

    TwoComponentWorld first, second;
    CHECK(first.world.GetComponentType<Position>() == second.world.GetComponentType<Position>());
}

TEST_CASE("Registering a component type again keeps the components already added") {
    TwoComponentWorld w;
    w.world.RegisterComponent<Position>();
    REQUIRE(w.world.HasComponent<Position>(w.both));
    CHECK(w.world.GetComponent<Position>(w.both).x == 1.0f);
}
