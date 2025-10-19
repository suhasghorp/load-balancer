#!/bin/bash

# Script to start backend servers

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PID_DIR="$SCRIPT_DIR/.pids"

# Create PID directory if it doesn't exist
mkdir -p "$PID_DIR"

# Find backend_server binary (prefer release, fallback to debug)
if [ -f "$SCRIPT_DIR/build-release/backend_server" ]; then
    BACKEND_BIN="$SCRIPT_DIR/build-release/backend_server"
    BUILD_TYPE="release"
elif [ -f "$SCRIPT_DIR/build-debug/backend_server" ]; then
    BACKEND_BIN="$SCRIPT_DIR/build-debug/backend_server"
    BUILD_TYPE="debug"
elif [ -f "$SCRIPT_DIR/build/backend_server" ]; then
    BACKEND_BIN="$SCRIPT_DIR/build/backend_server"
    BUILD_TYPE="default"
else
    echo "Error: backend_server binary not found"
    echo "Searched in: build-release/, build-debug/, build/"
    echo "Please build the project first:"
    echo "  mkdir build-debug && cd build-debug"
    echo "  cmake -DCMAKE_BUILD_TYPE=Debug .."
    echo "  cmake --build ."
    exit 1
fi

echo "Using $BUILD_TYPE build: $BACKEND_BIN"

# Function to start a single backend
start_backend() {
    local port=$1
    local pid_file="$PID_DIR/backend_$port.pid"

    # Check if already running
    if [ -f "$pid_file" ]; then
        local pid=$(cat "$pid_file")
        if ps -p "$pid" > /dev/null 2>&1; then
            echo "Backend on port $port is already running (PID: $pid)"
            return
        fi
    fi

    # Start the backend
    "$BACKEND_BIN" "$port" > /dev/null 2>&1 &
    local pid=$!
    echo "$pid" > "$pid_file"
    echo "Started backend server on port $port (PID: $pid)"
}

# Function to start all backends
start_all() {
    echo "Starting all backend servers (ports 8080-8089)..."
    for port in {8080..8089}; do
        start_backend "$port"
    done
    echo "All backend servers started"
}

# Main script
if [ $# -eq 0 ]; then
    start_all
elif [ "$1" = "all" ]; then
    start_all
else
    # Start specific port
    port=$1
    if [[ "$port" =~ ^[0-9]+$ ]] && [ "$port" -ge 8080 ] && [ "$port" -le 8089 ]; then
        start_backend "$port"
    else
        echo "Error: Invalid port $port. Must be between 8080-8089"
        exit 1
    fi
fi
