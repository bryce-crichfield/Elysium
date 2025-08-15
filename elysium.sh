#!/bin/bash

# Get the root path
ELYSIUM_ROOT="$(dirname "${BASH_SOURCE[0]}")"

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
    echo "Building Elysium..."
    pushd . > /dev/null

    # Create necessary directories
    mkdir -p "$ELYSIUM_ROOT/Build"
    mkdir -p "$ELYSIUM_ROOT/Binary"

    cd "$ELYSIUM_ROOT/Build"
    cmake .. && make
    popd > /dev/null
}

Elysium_Run() {
    pushd . > /dev/null
    cd "$ELYSIUM_ROOT/Binary"
    ./Elysium &
    popd > /dev/null
}

# Show help function
show_help() {
    echo "Usage: $0 [--clean] [--build] [--run] [--help]"
    echo "  --clean: Clean build artifacts"
    echo "  --build: Build the project"
    echo "  --run: Run the executable"
    echo "  --help: Show this help message"
    echo ""
    echo "Operations are executed in order: Clean -> Build -> Run"
}

# Parse command-line arguments
should_setup=false
should_clean=false
should_build=false
should_run=false

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
        --help)
            show_help
            exit 0
            ;;
        *)
            echo "Warning: Unknown argument: $arg" >&2
            echo "Use --help for usage information"
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
    Elysium_Build
fi

if [ "$should_run" = true ]; then
    Elysium_Run
fi

# If no arguments provided, show help
if [ $# -eq 0 ]; then
    echo "No arguments provided. Use --help for usage information."
    echo "Available options: --clean, --build, --run"
fi
