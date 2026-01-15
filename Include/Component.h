#pragma once

#include "raylib.h"
#include "Sprite.h"
#include <queue>
#include <memory>
#include <string>
namespace Elysium {

// Forward declarations
using Entity = size_t;

struct NameComponent {
    std::string name;
    NameComponent(const std::string& name = "");
};

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
    bool isMoving = false;
    bool loop = true;              // should it loop back to start when done?

    void AddWaypoint(const Vector2& waypoint);
    void ClearWaypoints();

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

enum class Direction {
    NONE = 0,
    UP,
    DOWN,
    LEFT,
    RIGHT
};

struct DirectionComponent {
    Direction currentDirection = Direction::NONE;
    Direction previousDirection = Direction::NONE;
    bool hasChanged = false;

    DirectionComponent() = default;
    DirectionComponent(Direction dir) : currentDirection(dir), previousDirection(dir) {}

    void SetDirection(Direction newDir) {
        if (newDir != currentDirection) {
            previousDirection = currentDirection;
            currentDirection = newDir;
            hasChanged = true;
        }
    }

    void ClearChanged() { hasChanged = false; }
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

struct CameraComponent
{
    // Expects PositionComponent
    Rectangle viewport;
    float zoom = 1.0f;
    std::vector<int> layerMask;         // which layers this camera renders
    int renderOrder = 0;                // for multi-camera setups
    bool isVisible = true;

    CameraComponent();
};

struct FollowComponent
{
    float speed = 1.0f;
    std::string targetEntityName;
};

struct TileComponent
{
    // ACTS LIKE A TYPE FLAG
    TileComponent() = default;
};

struct TeamComponent
{
    int teamId;

    TeamComponent();
    TeamComponent(int teamId);
};

struct CooldownComponent
{
    float cooldownTime = 0.0f;          // Time in seconds
    float elapsedTime = 0.0f;           // Time elapsed since last action
    bool isOnCooldown = false;           // Is the action currently on cooldown

    void SetCooldown(float duration);

    CooldownComponent();
    CooldownComponent(float cooldown);
};

/**
 *  In our game, characters are stored in a database.  All of the information about a character's
 *  attributes, skills, and equipment is stored here.
 *
 *  It's weird, but we won't store that info in the ECS.  We will keep that in the database.
 *  Therefore character component is just an access point for that database.  We just need an id.
 */
struct CharacterComponent {
    int id;
};

/**
 *  In our game, units are the entities that the player controls.  They usually have a character
 *  component that links them to the character database, and they store information about the unit's
 *  state in battle including turn state, status effects, and other battle-only modifiers
 */
struct UnitComponent {
    bool hasActedThisTurn;
    bool canMove;
    bool canAttack;
    bool canCastSpells;
    bool canUseItems;

    UnitComponent()
        : hasActedThisTurn(false)
        , canMove(true)
        , canAttack(true)
        , canCastSpells(true)
        , canUseItems(true)
        {}

    void StartTurn() {
        hasActedThisTurn = false;
    }

    void EndTurn() {
        hasActedThisTurn = true;
    }
};

/**
 * BoundsComponent stores the bounding rectangle for an entity.
 * Automatically computed by RenderSystem based on the entity's renderable component.
 * Used by PickSystem for mouse picking and debugging visualization.
 */
struct BoundsComponent {
    Rectangle bounds;           // Bounding box in world space
    bool isDragging;            // Is this entity currently being dragged
    Color debugColor;           // Color to draw debug bounds

    BoundsComponent()
        : bounds({0, 0, 0, 0})
        , isDragging(false)
        , debugColor(RED)
    {}

    BoundsComponent(Rectangle rect, Color color)
        : bounds(rect)
        , isDragging(false)
        , debugColor(color)
    {}
};

};
