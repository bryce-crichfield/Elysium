# Elysium Game Engine - Claude Working Notes

## Project Overview
Elysium is a C++ 2D game engine built with:
- **Raylib** for graphics and windowing
- **Entity Component System (ECS)** architecture
- **XML-driven** configuration and scene loading
- **Service-based architecture** with dependency injection
- **ImGUI Realtime Editing** enables content creation and debugging

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
# Setup once
.\elysium.ps1 --Setup

# Build
.\elysium.ps1 --Build

# Run
.\elysium.ps1 --Run
```

## Code Quality Standards

### Modern C++ (C++17)
- **RAII** for resource management
- **Smart pointers** for ownership
- **const-correctness** throughout
- **Exception handling** with proper bounds checking
- **Type aliases** for complex template types

### Naming Conventions
- **Classes**: PascalCase (`World`)
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
