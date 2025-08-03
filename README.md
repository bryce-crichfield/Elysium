# Elysium 
A C++ game engine built for 2D style game.

## Project Structure

```
Elysium/
├── Include/             # Header files
│   ├── Services/        # Service interfaces (EventService, AssetService, etc.)
│   ├── Scenes/          # Scene implementations
│   ├── Application.h    # Core application singleton
│   └── Scene.h          # Base scene class
├── Source/              # Implementation files
│   ├── Services/        # Service implementations
│   ├── Scenes/          # Scene logic (GameScene, MenuScene)
│   ├── Application.cpp  # Main application loop
│   └── main.cpp         # Entry point
├── Assets/              # Game assets (copied to Binary/Assets)
├── Binary/              # Compiled executables and runtime assets
├── Build/               # CMake build files
├── Vendor/              # Third-party libraries
│   ├── raylib/          # Graphics and game framework
│   ├── imgui/           # Immediate mode GUI
│   ├── rlImGui/         # Raylib ImGui integration
│   └── tinyxml2/        # XML parsing
└── CMakeLists.txt       # CMake configuration
```

### Core Architecture

**Namespace: `Elysium`**
- **Application** - Singleton managing window, services, and scene transitions
- **Scene** - Base class for game states (menu, gameplay, etc.)

**Namespace: `Elysium::Services`**
- **EventService** - Input and network event handling
- **AssetService** - Resource loading with caching
- **NetworkService** - Multiplayer connectivity (placeholder)
- **MetricsService** - Performance monitoring (F2 to toggle)
- **LogService** - Logging with file output and overlay (F3 to toggle)

## Build System

Uses CMake with the following setup:
- **C++17** standard
- Outputs to `Binary/` directory  
- Automatically copies `Assets/` to `Binary/Assets/`
- Links: raylib, ImGui, rlImGui, tinyxml2

## Quick Start

### Linux/Mac

```bash
# Source the build script
source ./elysium.sh

# Clean, build, and run
Elysium_Clean
Elysium_Build  
Elysium_Run
```

### Windows (PowerShell)

```powershell
# Dot-source the build script
. .\elysium.ps1

# Clean, build, and run
Elysium_Clean
Elysium_Build
Elysium_Run
```

## Manual Build

```bash
mkdir -p Build Binary
cd Build
cmake ..
make
cd ../Binary
./Elysium
```

## Available Commands

After sourcing the appropriate script:

- `Elysium_Clean` - Remove Build/ and Binary/ directories
- `Elysium_Build` - Configure with CMake and compile
- `Elysium_Run` - Execute the built binary

## Requirements

- CMake 3.20+
- C++17 compatible compiler
- OpenGL support (for Raylib)