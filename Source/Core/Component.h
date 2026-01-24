#pragma once

#include <memory>
#include <queue>
#include <string>
#include "Sprite.h"
#include "raylib.h"
#include "Core/Event.h"
#include "Core/Signal.h"
namespace Elysium {

// Forward declarations
using Entity = size_t;

struct NameComponent {
    std::string name;
    NameComponent(const std::string& name = "");
};

// Component structs
struct PositionComponent {
    float x, y;
    PositionComponent(float x = 0.0f, float y = 0.0f);
};

struct ScaleComponent {
    float x = 1.0f;
    float y = 1.0f;

    ScaleComponent() = default;
    ScaleComponent(float x, float y) : x(x), y(y) {}
    ScaleComponent(float uniform) : x(uniform), y(uniform) {}
};

struct LocationComponent {
    int x, y;

    LocationComponent(int x = 0, int y = 0);
};

struct MovementComponent {
    std::vector<Vector2> waypoints;
    size_t currentWaypointIndex = 0;
    float speed = 100.0f;
    bool isMoving = false;
    bool loop = true;  // should it loop back to start when done?

    void AddWaypoint(const Vector2& waypoint) {
        waypoints.push_back(waypoint);
    }
    void ClearWaypoints() {
        waypoints.clear();
    }

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
    };
    ;

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

    Vector2 parallaxFactor = {0.0f, 0.0f};  // 0 = no movement, 1 = full camera movement

    std::vector<Entity> allowedCameras;  // which camera entities can see this layer (empty = all)

    RenderTexture2D* framebuffer = nullptr;  // optional framebuffer to draw this layer to
    LayerComponent(int z = 0);
};

struct RectangleComponent {
    float width, height;
    Color background;
    Color border;
    std::string layerName = "default";

    RectangleComponent(float width = 1, float height = 1, Color background = {}, Color border = {}, const std::string& layer = "default");
};

struct CircleComponent {
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

struct SpriteComponent {
    Sprite sprite;
    std::string markerName;
    std::string layerName = "default";
    int frameIndex = 0;          // Current frame within the marker
    float frameDuration = 0.2f;  // Time per frame in seconds
    float frameElapsed = 0.0f;   // Time elapsed on current frame

    SpriteComponent() = default;
    SpriteComponent(const Sprite& sprite, const std::string& marker, const std::string& layer = "default");
};

struct TextureComponent {
    std::string textureName;  // Asset name of the texture
    std::string layerName = "default";
    Rectangle clip = {0, 0, 0, 0};  // Source clip rect (0,0,0,0 = use full texture)
    Color tint = WHITE;             // Tint/modulation color

    TextureComponent() = default;
    TextureComponent(const std::string& texture, const std::string& layer = "default");
};

struct TextComponent {
    std::string content;
    int fontSize;
    Color color;
    std::string layerName = "default";

    TextComponent(const std::string& text = "", int size = 20, Color c = {}, const std::string& layer = "default");
};

struct CameraComponent {
    // Expects PositionComponent
    Rectangle viewport;
    float zoom = 1.0f;
    std::vector<int> layerMask;  // which layers this camera renders
    int renderOrder = 0;         // for multi-camera setups
    bool isVisible = true;

    CameraComponent();
};

struct FollowComponent {
    float speed = 1.0f;
    std::string targetEntityName;
};

struct TileComponent {
    // ACTS LIKE A TYPE FLAG
    TileComponent() = default;
};

struct TeamComponent {
    int teamId;

    TeamComponent();
    TeamComponent(int teamId);
};

struct CooldownComponent {
    float cooldownTime = 0.0f;  // Time in seconds
    float elapsedTime = 0.0f;   // Time elapsed since last action
    bool isOnCooldown = false;  // Is the action currently on cooldown

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
        : hasActedThisTurn(false), canMove(true), canAttack(true), canCastSpells(true), canUseItems(true) {}

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
    Rectangle bounds;  // Bounding box in world space
    bool isDragging;   // Is this entity currently being dragged
    Color debugColor;  // Color to draw debug bounds

    BoundsComponent()
        : bounds({0, 0, 0, 0}), isDragging(false), debugColor(RED) {}

    BoundsComponent(Rectangle rect, Color color)
        : bounds(rect), isDragging(false), debugColor(color) {}

    Signal<Event> OnPickEvent;
};

struct PathRequestComponent {
    Vector2 target;
    PathRequestComponent() : target({0.0f, 0.0f}) {}
    PathRequestComponent(Vector2 t) : target(t) {}
};

/**
 * SelectionComponent is a tag component - its presence indicates the entity is selected.
 * Add to select, remove to deselect.
 */
struct SelectionComponent {
    SelectionComponent() = default;
};

struct ScriptComponent {
    std::string scriptName;
    bool isActive = true;
    bool isInitialized = false;

    ScriptComponent() = default;
    ScriptComponent(const std::string& name) : scriptName(name) {}
};

/**
 * KinematicsComponent handles physics-based movement.
 * Stores velocity, acceleration, and damping (friction).
 */
struct KinematicsComponent {
    Vector2 velocity;
    Vector2 acceleration;
    float friction; // 0.0 = no friction, 1.0 = instant stop per second
    float maxSpeed;

    KinematicsComponent(float maxSpd = 200.0f, float f = 5.0f) 
        : velocity({0,0}), acceleration({0,0}), friction(f), maxSpeed(maxSpd) {}
};

struct HealthComponent {
    float current;
    float max;
    
    HealthComponent(float maxHealth = 100.0f) : current(maxHealth), max(maxHealth) {}
};

struct FactionComponent {
    std::string name;
    
    FactionComponent(const std::string& name = "Player") : name(name) {}
};

struct AttackComponent {
    float range;
    float damage;
    float cooldown; // seconds
    float timer;    // current countdown
    Entity targetId;
    bool isAttacking;
    
    AttackComponent(float rng = 100.0f, float dmg = 10.0f, float cd = 1.0f) 
        : range(rng), damage(dmg), cooldown(cd), timer(0.0f), targetId(0), isAttacking(false) {}
};

struct ProjectileComponent {
    float damage;
    Entity targetId;
    float speed;
    float lifetime; // seconds before auto-destroy
    
    ProjectileComponent(float dmg = 10.0f, Entity target = 0, float spd = 300.0f)
        : damage(dmg), targetId(target), speed(spd), lifetime(5.0f) {}
};

};  // namespace Elysium
