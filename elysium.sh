#!/bin/bash

# Get the root path
ELYSIUM_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

Elysium_Setup() {
    echo "Setting up Elysium environment..."
}

# Function definitions
Elysium_Clean() {
    echo "Cleaning build artifacts..."
    rm -rf "$ELYSIUM_ROOT/Build"
    rm -rf "$ELYSIUM_ROOT/Binary"
    echo "Clean complete"
}

Elysium_Build() {
    local tracy=$1
    local mode=$2
    local target=$3
    # Release is CMake's RelWithDebInfo: -O2, with symbols kept so Tracy and gdb still read it.
    local build_type="Debug"
    if [ "$mode" = "Release" ]; then
        build_type="RelWithDebInfo"
    fi

    echo "Building Elysium ($mode)..."
    pushd . > /dev/null

    # Create necessary directories
    mkdir -p "$ELYSIUM_ROOT/Build"
    mkdir -p "$ELYSIUM_ROOT/Binary"

    cd "$ELYSIUM_ROOT"

    # --Tracy turns the profiler on. Its worker threads sleep through winpthreads, and the
    # statically linked winpthreads in this mingw-w64/GCC build corrupts the stack canary in
    # nanosleep ("stack smashing detected" on the "Tracy DXT1" thread). So a Tracy build
    # links winpthreads dynamically and ships libwinpthread-1.dll beside the exe.
    local tracy_option="OFF"
    local link_static="-static -static-libgcc -static-libstdc++"
    if [ "$tracy" = "true" ]; then
        tracy_option="ON"
        link_static="-static-libgcc -static-libstdc++"
    fi

    cmake -G "Ninja" -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DCMAKE_MAKE_PROGRAM=ninja \
        -DCMAKE_BUILD_TYPE="$build_type" -DTRACY_ENABLE="$tracy_option" \
        -DCMAKE_EXE_LINKER_FLAGS="$link_static" -B Build

    if [ $? -eq 0 ]; then
        # --Target builds one layer (ElysiumCore, ElysiumServices, ElysiumEditor) or the Elysium exe.
        if [ -n "$target" ]; then
            cmake --build Build --target "$target"
        else
            cmake --build Build
        fi
    fi

    popd > /dev/null
}

Elysium_Test() {
    "$ELYSIUM_ROOT/Binary/ElysiumTests" || exit $?
}

Elysium_Run() {
    local project_path=$1
    local editor_mode=$2

    if [ -z "$project_path" ]; then
        echo "Error: Project path is required. Specify it with --Project=<path>." >&2
        exit 1
    fi

    # Trim trailing slashes and append Project.xml
    project_path="${project_path%/}/Project.xml"

    # Resolve to absolute path if relative
    if [[ "$project_path" != /* ]]; then
        project_path="$ELYSIUM_ROOT/$project_path"
    fi

    if [ ! -f "$project_path" ]; then
        echo "Error: Project file not found: $project_path" >&2
        exit 1
    fi

    pushd . > /dev/null
    cd "$ELYSIUM_ROOT/Binary"

    local exe_args="--Project=$project_path"
    if [ "$editor_mode" = "true" ]; then
        exe_args="$exe_args --Editor=true"
    fi

    ./Elysium $exe_args &
    popd > /dev/null
}

# Show help function
show_help() {
    echo "Usage: $0 [--Clean] [--Build] [--Tracy] [--Mode=Debug|Release] [--Target=<name>] [--Test] [--Run] [--Project=<path>] [--Editor] [--Help]"
    echo "  --Clean: Clean build artifacts"
    echo "  --Build: Build the project"
    echo "  --Tracy: Build with the Tracy profiler (v0.12.2) enabled"
    echo "  --Mode=Debug|Release: Build type (default Debug); Release is optimized (-O2) with symbols"
    echo "  --Target=<name>: Build only this target: ElysiumCore, ElysiumServices, ElysiumEditor or Elysium (default: everything)"
    echo "  --Test: Run the unit tests (Binary/ElysiumTests); exits non-zero on failure"
    echo "  --Run: Run the executable"
    echo "  --Project=<path>: Project directory to load, e.g. Projects/DemoGame (required for --Run)"
    echo "  --Editor: Start in editor mode"
    echo "  --Help: Show this help message"
    echo ""
    echo "Operations are executed in order: Clean -> Build -> Test -> Run"
}

# Parse command-line arguments
should_setup=false
should_clean=false
should_build=false
should_run=false
should_test=false
tracy_arg=false
mode_arg="Debug"
target_arg=""
project_arg=""
editor_arg=false

# Convert arguments to lowercase for case-insensitive matching
for arg in "$@"; do
    case "${arg,,}" in
        --setup)
            should_setup=true
            ;;
        --clean)
            should_clean=true
            ;;
        --build)
            should_build=true
            ;;
        --run)
            should_run=true
            ;;
        --test)
            should_test=true
            ;;
        --tracy)
            tracy_arg=true
            ;;
        --mode=debug)
            mode_arg="Debug"
            ;;
        --mode=release)
            mode_arg="Release"
            ;;
        --target=*)
            target_arg="${arg#*=}"
            ;;
        --project=*)
            project_arg="${arg#*=}"
            ;;
        --editor)
            editor_arg=true
            ;;
        --editor=true)
            editor_arg=true
            ;;
        --editor=false)
            editor_arg=false
            ;;
        --help)
            show_help
            exit 0
            ;;
        *)
            echo "Warning: Unknown argument: $arg" >&2
            echo "Use --Help for usage information"
            ;;
    esac
done

# Execute operations in the correct order
if [ "$should_setup" = true ]; then
    Elysium_Setup
fi

if [ "$should_clean" = true ]; then
    Elysium_Clean
fi

if [ "$should_build" = true ]; then
    Elysium_Build "$tracy_arg" "$mode_arg" "$target_arg"
fi

if [ "$should_test" = true ]; then
    Elysium_Test
fi

if [ "$should_run" = true ]; then
    Elysium_Run "$project_arg" "$editor_arg"
fi

# If no arguments provided, show help
if [ $# -eq 0 ]; then
    echo "No arguments provided. Use --Help for usage information."
    echo "Available options: --Clean, --Build, --Test, --Run"
fi
