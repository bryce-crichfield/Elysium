# ECS Module User Manual

## Overview

This ECS (Entity Component System) module provides a high-performance, memory-tracked architecture for game development using Raylib. It follows the ECS pattern where entities are composed of components and operated on by systems.

## Core Concepts

- **Entity**: A unique identifier (size_t) representing a game object
- **Component**: Data structures that define properties (Position, Velocity, etc.)
- **System**: Logic that operates on entities with specific component combinations
- **World**: The main container managing all entities and components

## Getting Started

```cpp
#include "ECS.h"

// Create the world
EntityWorld world;

// Create an entity
Entity player = world.CreateEntity();

// Add components
world.AddComponent(player, PositionComponent(100.0f, 200.0f));
world.AddComponent(player, VelocityComponent(5.0f, 0.0f));
world.AddComponent(player, CircleRenderComponent(25.0f, BLUE));
```

## Built-in Components

### PositionComponent
```cpp
struct PositionComponent {
    float x, y;
    PositionComponent(float x = 0.0f, float y = 0.0f);
};
```
### VelocityComponent

```cpp
struct VelocityComponent {
    float x, y;
    VelocityComponent(float x = 0.0f, float y = 0.0f);
};
```

### CircleRenderComponent
```cpp
struct CircleRenderComponent {
    float radius;
    Color color;
    bool drawOutline;
    Color outlineColor;
    
    CircleRenderComponent(float r = 10.0f, Color c = WHITE, 
                         bool outline = true, Color outlineC = BLACK);
};
```

### PhysicsComponent

```cpp
struct PhysicsComponent {
    float mass;
    float restitution; // bounciness (0-1)
    float friction;
    bool affectedByGravity;
    
    PhysicsComponent(float m = 1.0f, float rest = 0.8f, 
                    float fric = 0.1f, bool gravity = true);
};
```

### SpriteComponent

```cpp
struct SpriteComponent {
    std::string textureName;
    std::string frame;
    Vector2 scale;
    float rotation;
    Color tint;
    
    SpriteComponent(const std::string& name = "", const std::string& f = "", 
                   Vector2 s = {1.0f, 1.0f}, float rot = 0.0f, Color t = WHITE);
};
```

### TextComponent

```cpp
struct TextComponent {
    std::string content;
    int fontSize;
    Color color;
    
    TextComponent(const std::string& text = "", int size = 20, Color c = BLACK);
};
```

## Method Glossary

### EntityWorld Methods

#### Entity Management
- `Entity CreateEntity()` - Creates a new entity and returns its ID
- `void DestroyEntity(Entity entity)` - Destroys an entity and all its components
- `size_t GetEntityCount() const` - Returns the number of living entities

#### Component Management
- `void RegisterComponent<T>()` - Registers a new component type with the system
- `void AddComponent<T>(Entity entity, T component)` - Adds a component to an entity
- `void RemoveComponent<T>(Entity entity)` - Removes a component from an entity
- `T& GetComponent<T>(Entity entity)` - Gets a mutable reference to a component
- `const T& GetComponent<T>(Entity entity) const` - Gets a const reference to a component
- `bool HasComponent<T>(Entity entity) const` - Checks if an entity has a specific component
- `size_t GetComponentType<T>() const` - Returns the type ID for a component

#### Entity Queries
- `const TrackedVector<Entity>& GetEntitiesWithComponents<ComponentTypes...>() const` - Returns entities with specified components
- `void ForEachEntityWith<ComponentTypes...>(Func&& func) const` - Iterates over entities with specified components using a lambda

### System Base Class

#### Virtual Methods to Override
- `virtual void Update(float deltaTime) = 0` - Called every frame for game logic
- `virtual void Render()` - Called every frame for rendering
- `virtual void OnEvent(const char* eventName, void* data)` - Called when events are triggered

## Code Recipes

### Creating a Custom Component

```cpp
struct HealthComponent {
    int currentHealth;
    int maxHealth;
    bool invulnerable;
    
    HealthComponent(int max = 100) 
        : currentHealth(max), maxHealth(max), invulnerable(false) {}
};

// Register the component
world.RegisterComponent<HealthComponent>();

// Use it
Entity enemy = world.CreateEntity();
world.AddComponent(enemy, HealthComponent(50));
```

### Creating a Movement System

```cpp
class MovementSystem : public System {
public:
    MovementSystem(EntityWorld* world) : System(world) {}
    
    void Update(float deltaTime) override {
        world->ForEachEntityWith<PositionComponent, VelocityComponent>(
            [this, deltaTime](Entity entity) {
                auto& pos = world->GetComponent<PositionComponent>(entity);
                auto& vel = world->GetComponent<VelocityComponent>(entity);
                
                pos.x += vel.x * deltaTime;
                pos.y += vel.y * deltaTime;
            }
        );
    }
};
```

### Creating a Render System

```cpp
class RenderSystem : public System {
public:
    RenderSystem(EntityWorld* world) : System(world) {}
    
    void Render() override {
        // Render circles
        world->ForEachEntityWith<PositionComponent, CircleRenderComponent>(
            [this](Entity entity) {
                auto& pos = world->GetComponent<PositionComponent>(entity);
                auto& circle = world->GetComponent<CircleRenderComponent>(entity);
                
                DrawCircle(pos.x, pos.y, circle.radius, circle.color);
                if (circle.drawOutline) {
                    DrawCircleLines(pos.x, pos.y, circle.radius, circle.outlineColor);
                }
            }
        );
        
        // Render text
        world->ForEachEntityWith<PositionComponent, TextComponent>(
            [this](Entity entity) {
                auto& pos = world->GetComponent<PositionComponent>(entity);
                auto& text = world->GetComponent<TextComponent>(entity);
                
                DrawText(text.content.c_str(), pos.x, pos.y, text.fontSize, text.color);
            }
        );
    }
};
```

### Physics System Example

```cpp
class PhysicsSystem : public System {
public:
    PhysicsSystem(EntityWorld* world) : System(world) {}
    
    void Update(float deltaTime) override {
        const float GRAVITY = 980.0f; // pixels per second squared
        
        world->ForEachEntityWith<PositionComponent, VelocityComponent, PhysicsComponent>(
            [this, deltaTime, GRAVITY](Entity entity) {
                auto& pos = world->GetComponent<PositionComponent>(entity);
                auto& vel = world->GetComponent<VelocityComponent>(entity);
                auto& physics = world->GetComponent<PhysicsComponent>(entity);
                
                // Apply gravity
                if (physics.affectedByGravity) {
                    vel.y += GRAVITY * deltaTime;
                }
                
                // Simple ground collision
                if (pos.y > 600 && vel.y > 0) {
                    pos.y = 600;
                    vel.y *= -physics.restitution;
                    vel.x *= (1.0f - physics.friction);
                }
            }
        );
    }
};
```

### Complete Game Loop Setup

```cpp
int main() {
    InitWindow(800, 600, "ECS Example");
    SetTargetFPS(60);
    
    // Create world and systems
    EntityWorld world;
    MovementSystem movementSystem(&world);
    PhysicsSystem physicsSystem(&world);
    RenderSystem renderSystem(&world);
    
    // Create some entities
    Entity ball = world.CreateEntity();
    world.AddComponent(ball, PositionComponent(400, 100));
    world.AddComponent(ball, VelocityComponent(100, 0));
    world.AddComponent(ball, CircleRenderComponent(20, RED));
    world.AddComponent(ball, PhysicsComponent(1.0f, 0.8f, 0.1f, true));
    
    Entity staticText = world.CreateEntity();
    world.AddComponent(staticText, PositionComponent(10, 10));
    world.AddComponent(staticText, TextComponent("ECS Demo", 24, DARKBLUE));
    
    while (!WindowShouldClose()) {
        float deltaTime = GetFrameTime();
        
        // Update systems
        movementSystem.Update(deltaTime);
        physicsSystem.Update(deltaTime);
        
        // Render
        BeginDrawing();
        ClearBackground(RAYWHITE);
        
        renderSystem.Render();
        
        EndDrawing();
    }
    
    CloseWindow();
    return 0;
}
```

### Entity Spawning Helper

```cpp
Entity SpawnBall(EntityWorld& world, float x, float y, Color color) {
    Entity entity = world.CreateEntity();
    world.AddComponent(entity, PositionComponent(x, y));
    world.AddComponent(entity, VelocityComponent(
        GetRandomValue(-200, 200), 
        GetRandomValue(-100, -300)
    ));
    world.AddComponent(entity, CircleRenderComponent(
        GetRandomValue(10, 30), 
        color, 
        true, 
        BLACK
    ));
    world.AddComponent(entity, PhysicsComponent(1.0f, 0.7f, 0.05f, true));
    return entity;
}
```

### Component Querying Patterns

```cpp
// Check if entity has specific components
if (world.HasComponent<PositionComponent>(entity) && 
    world.HasComponent<VelocityComponent>(entity)) {
    // Safe to access these components
}

// Get all entities with multiple components
const auto& movableEntities = world.GetEntitiesWithComponents<
    PositionComponent, VelocityComponent>();

for (Entity entity : movableEntities) {
    auto& pos = world.GetComponent<PositionComponent>(entity);
    auto& vel = world.GetComponent<VelocityComponent>(entity);
    // Process entity
}

// Using lambda iteration (preferred for performance)
world.ForEachEntityWith<PositionComponent, VelocityComponent>(
    [&](Entity entity) {
        // Direct access to components
        auto& pos = world.GetComponent<PositionComponent>(entity);
        auto& vel = world.GetComponent<VelocityComponent>(entity);
    }
);
```

## Performance Tips

- **Use ForEachEntityWith**: Prefer lambda-based iteration over getting entity lists
- **Pre-allocate**: The system pre-allocates query buffers to avoid memory allocations
- **Component Design**: Keep components as simple data structures
- **System Organization**: Group related logic into dedicated systems
- **Memory Tracking**: All containers use tracked allocators for memory profiling

## Constants and Limits

- `MAX_ENTITIES`: 10,000 entities maximum
- `MAX_COMPONENTS`: 32 different component types maximum
- `INVALID_ENTITY`: Returned when entity creation fails

## Error Handling

- Component access on non-existent entities throws `std::runtime_error`
- Entity creation returns `INVALID_ENTITY` when limit exceeded
- Component operations are safe with bounds checking



