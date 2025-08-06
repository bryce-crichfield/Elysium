# Elysium Game Engine - Claude Working Notes

## Project Overview
Elysium is a C++ 2D game engine built with:
- **Raylib** for graphics and windowing
- **Entity Component System (ECS)** architecture
- **XML-driven** configuration and scene loading
- **Custom memory tracking** with TrackedAllocator
- **Service-based architecture** with dependency injection

## Project Structure

```
Elysium2/
├── Include/              # Header files
│   ├── Application.h     # Core application singleton
│   ├── Entity.h         # ECS implementation (Entity, Components, Systems)
│   ├── Scene.h          # Base scene class
│   ├── Asset.h          # Asset management types
│   ├── Scenes/          # Scene implementations
│   ├── Services/        # Service interfaces
│   └── Systems/         # ECS system implementations
├── Source/              # Implementation files
│   ├── main.cpp         # Entry point
│   ├── Application.cpp  # Application logic
│   ├── Scenes/          # Scene implementations
│   ├── Services/        # Service implementations
│   └── Systems/         # System implementations
├── Assets/              # Game assets and config files
│   ├── Config/          # XML configuration files
│   ├── textures/        # Image assets
│   ├── sounds/          # Audio assets
│   └── Scene.xml        # Scene definitions
├── Documents/           # Documentation
│   └── ecs.md          # ECS usage manual
└── Vendor/             # Third-party libraries
```

## Build System

### Windows (PowerShell)
```powershell
# Source the build script
. .\elysium.ps1

# Available commands:
Elysium_Clean    # Remove Build/ and Binary/ directories
Elysium_Build    # Configure with CMake and compile
Elysium_Run      # Execute the built binary
```

### Linux/Mac
```bash
source ./elysium.sh
Elysium_Clean && Elysium_Build && Elysium_Run
```

### Manual Build
```bash
mkdir -p Build Binary
cd Build
cmake ..
make
cd ../Binary
./Elysium
```

## Architecture Patterns

### ECS Implementation
- **Entities**: Lightweight IDs (size_t), max 10,000
- **Components**: Plain data structs (Position, Velocity, Physics, Rendering)
- **Systems**: Logic processors that operate on component combinations
- **EntityWorld**: Central coordinator managing all ECS operations

Key ECS files:
- `Include/Entity.h` - Complete ECS implementation
- `Include/Systems/` - System base classes and implementations
- `Documents/ecs.md` - Comprehensive usage guide

### Service Architecture
All services accessible via `Application::GetInstance()`:
- **EventService** - Input and network events
- **AssetService** - Resource loading with caching  
- **LoadingService** - Asset loading orchestration
- **JukeboxService** - Audio playback management
- **NetworkService** - Multiplayer (placeholder)
- **MetricsService** - Performance monitoring (F2 key)
- **LogService** - Logging with overlay (F3 key)

### Scene System
- **Base Scene class** with ECS integration
- **XML-driven** scene loading from `Assets/Scene.xml`
- **Factory pattern** with scene type registration
- Each scene owns its `EntityWorld` instance

## XML Configuration

### Application Config (`Assets/Config/ApplicationConfig.xml`)
```xml
<GameConfig>
    <Window>
        <Width>666</Width>
        <Height>666</Height>
        <!-- ... more window settings -->
    </Window>
    <Debug>
        <ShowDemoWindow>true</ShowDemoWindow>
        <ShowMetrics>false</ShowMetrics>
    </Debug>
</GameConfig>
```

### Scene Definitions (`Assets/Scene.xml`)
```xml
<Scene type="BattleScene">
    <Systems>
        <System type="PhysicsSystem" gravity="500.0" />
        <System type="RenderSystem" />
    </Systems>
    <Entities>
        <Entity name="Player">
            <PositionComponent x="100" y="100"/>
            <VelocityComponent x="0" y="0"/>
            <!-- ... more components -->
        </Entity>
    </Entities>
</Scene>
```

## Memory Management

### TrackedAllocator System
- **Custom allocators** for all ECS containers
- **Memory tracking** via `MemoryTracker::GetInstance()`
- **Type aliases** for cleaner template declarations:
  - `TrackedVector<T>`
  - `TrackedUnorderedMap<Key, Value>`
  - `TrackedDeque<T>`

### Performance Optimizations
- **Packed component arrays** for cache efficiency
- **Component masks** with bitwise operations
- **Pre-allocated query buffers** to avoid allocations
- **Living entity tracking** for fast iteration

## Development Guidelines

### Adding New Scenes
1. Create header in `Include/Scenes/YourScene.h`
2. Implement in `Source/Scenes/YourScene.cpp`
3. Register in `main.cpp`: `app.DefineScene("YourScene", factory)`
4. Optional: Create XML config in `Assets/`

### Adding New Components
1. Define struct in `Entity.h` 
2. Register in `EntityWorld` constructor
3. Update any relevant systems
4. Update `Documents/ecs.md` documentation

### Adding New Services
1. Create interface in `Include/Services/`
2. Implement in `Source/Services/`
3. Add to `Application.h` as member
4. Initialize in `Application::Initialize()`

## Code Quality Standards

### Modern C++ (C++17)
- **RAII** for resource management
- **Smart pointers** for ownership
- **const-correctness** throughout
- **Exception handling** with proper bounds checking
- **Type aliases** for complex template types

### Naming Conventions
- **Classes**: PascalCase (`EntityWorld`)
- **Methods**: PascalCase (`CreateEntity`)
- **Variables**: camelCase with trailing underscore for members (`world_`)
- **Constants**: UPPER_SNAKE_CASE (`MAX_ENTITIES`)
- **Namespaces**: `Elysium::Services`, `Elysium::Scenes`

### Build Dependencies
- **CMake 3.20+**
- **C++17 compatible compiler**
- **OpenGL support** (for Raylib)
- **Windows**: MinGW64 or Visual Studio
- **Linux/Mac**: GCC or Clang

## Testing & Debugging

### Built-in Tools
- **F2**: Toggle metrics overlay
- **F3**: Toggle log overlay  
- **ImGui integration** for debug interfaces
- **Memory tracking** for leak detection
- **XML validation** for config errors

### Common Issues
- **Asset paths**: Must be relative to `Binary/` directory
- **Scene registration**: Must happen before `SetScene()`
- **Component registration**: Must happen in `EntityWorld` constructor
- **Memory tracking**: Use TrackedAllocator types for ECS containers

## Recent Improvements Made
- Fixed template type alias declarations for readability
- Added proper const-correctness throughout ECS
- Enhanced error handling with bounds checking
- Improved brace formatting consistency
- Added INVALID_ENTITY constant for better error handling