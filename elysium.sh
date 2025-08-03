#!/bin/bash

ELYSIUM_ROOT="$(dirname "${BASH_SOURCE[0]}")"

Elysium_Clean() {
    echo "Cleaning build artifacts..."
    rm -rf "$ELYSIUM_ROOT/Build"
    rm -rf "$ELYSIUM_ROOT/Binary"
    echo "Clean complete"
}

Elysium_Build() {
    echo "Building Elysium..."
    pushd .
    
    # Create necessary directories
    mkdir -p "$ELYSIUM_ROOT/Build"
    mkdir -p "$ELYSIUM_ROOT/Binary"
    
    cd "$ELYSIUM_ROOT/Build"
    cmake .. && make
    popd
}

Elysium_Run() {
    pushd .
    cd $ELYSIUM_ROOT/Binary
    ./Elysium &
    popd
}

