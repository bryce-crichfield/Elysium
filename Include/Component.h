#pragma once

#include "raylib.h"
#include <queue>
#include <memory>
#include <string>

namespace Elysium {

// Forward declarations
struct Action;

// Component structs
struct PositionComponent
{
    float x, y;
    PositionComponent(float x = 0.0f, float y = 0.0f);
};

struct LocationComponent
{
    int x, y;
    LocationComponent(int x = 0, int y = 0);
};

struct VelocityComponent
{
    float x, y;
    VelocityComponent(float x = 0.0f, float y = 0.0f);
};

struct PhysicsComponent
{
    float mass;
    float restitution; // bounciness (0-1)
    float friction;
    bool affectedByGravity;

    PhysicsComponent(float m = 1.0f, float rest = 0.8f, float fric = 0.1f, bool gravity = true);
};

struct AnimationComponent
{
    std::queue<std::shared_ptr<Action>> actionQueue;
    std::shared_ptr<Action> currentAction = nullptr;

    AnimationComponent();
};

struct LayerComponent {
    int z;

    LayerComponent(int z = 0);
};

struct RectangleComponent
{
    float width, height;
    Color background;
    Color border;

    RectangleComponent(float width = 1, float height = 1, Color background = {}, Color border = {});
};

struct CircleComponent
{
    float radius;
    Color background;
    Color border;

    CircleComponent(float r = 10.0f, Color background = {}, Color border = {});
};

struct LightComponent {
    Color color;
    float radius;

    LightComponent(Color c = WHITE, float r = 50.0f) : color(c), radius(r) {}
};

struct SpriteComponent
{
    std::string textureName;
    std::string frame;
    Vector2 scale;
    float rotation;
    Color tint;

    SpriteComponent(const std::string& name = "", const std::string& f = "", Vector2 s = {1.0f, 1.0f}, float rot = 0.0f, Color t = {});
};

struct TextComponent
{
    std::string content;
    int fontSize;
    Color color;

    TextComponent(const std::string& text = "", int size = 20, Color c = {});
};

// Specialized components
struct CameraComponent
{
    std::string target;
    struct Camera2D camera; // Forward declaration or assume defined in raylib.h

    CameraComponent();
    CameraComponent(const std::string& target);
};

struct TileComponent
{
    // ACTS LIKE A TYPE FLAG
};

struct TeamComponent
{
    int teamId;

    TeamComponent();
    TeamComponent(int teamId);
};

};
