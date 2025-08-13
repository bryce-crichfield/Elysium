#pragma once

#include "raylib.h"
#include "Sprite.h"
#include <queue>
#include <memory>
#include <string>
namespace Elysium {

// Forward declarations
using Entity = size_t;

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

struct MovementComponent
{
    std::vector<Vector2> waypoints;
    size_t currentWaypointIndex = 0;
    float speed = 100.0f;
    bool isMoving = true;
    bool loop = true;              // should it loop back to start when done?

    MovementComponent() = default;
    MovementComponent(const std::vector<Vector2>& waypoints);
};

struct AnimationComponent {
    std::string marker;
    int currentFrame = 0;
    int start, end;
    float frameDuration, elapsed = 0;
    bool loop = false;

    AnimationComponent() = default;
    AnimationComponent(std::string marker);
};

struct LayerComponent {
    enum class Type {
        Background,
        World,
        Lighting,
        Overlay
    };

    enum class Space {
        World,
        Screen,
        Parallax
    };;

    enum class Blend {
        Normal,
        Additive,
        Multiply,
        Alpha
    };

    int zIndex;
    Type type;
    Space space;
    Blend blend;

    float opacity = 1.0f;
    bool isVisible = true;

    std::string name;

    Vector2 parallaxFactor = {0.0f, 0.0f};      // 0 = no movement, 1 = full camera movement

    std::vector<Entity> allowedCameras;         // which camera entities can see this layer (empty = all)

    RenderTexture2D* framebuffer = nullptr;     // optional framebuffer to draw this layer to
    LayerComponent(int z = 0);
};

struct RectangleComponent
{
    float width, height;
    Color background;
    Color border;
    std::string layerName = "default";

    RectangleComponent(float width = 1, float height = 1, Color background = {}, Color border = {}, const std::string& layer = "default");
};

struct CircleComponent
{
    float radius;
    Color background;
    Color border;
    std::string layerName = "default";

    CircleComponent(float r = 10.0f, Color background = {}, Color border = {}, const std::string& layer = "default");
};

struct LightComponent {
    Color color;
    float radius;
    std::string layerName = "default";

    LightComponent(Color c = WHITE, float r = 50.0f, const std::string& layer = "default") : color(c), radius(r), layerName(layer) {}
};

struct SpriteComponent
{
    Sprite sprite;
    std::string markerName;
    std::string layerName = "default";
    int frameIndex = 0;  // Current frame within the marker
    float frameDuration = 0.2f;  // Time per frame in seconds
    float frameElapsed = 0.0f;   // Time elapsed on current frame

    SpriteComponent() = default;
    SpriteComponent(const Sprite& sprite, const std::string& marker, const std::string& layer = "default");
};

struct TextComponent
{
    std::string content;
    int fontSize;
    Color color;
    std::string layerName = "default";

    TextComponent(const std::string& text = "", int size = 20, Color c = {}, const std::string& layer = "default");
};

// Specialized components
struct CameraComponent
{
    Vector2 position;                   // relative to entity's PositionComponent (for lerping)
    float zoom = 1.0f;
    Rectangle viewport;
    std::vector<int> layerMask;         // which layers this camera renders
    int renderOrder = 0;                // for multi-camera setups
    bool isVisible = true;

    CameraComponent();
};

struct FollowComponent
{
    std::string targetEntityName;
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
