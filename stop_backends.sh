#!/bin/bash

# Script to stop backend servers

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PID_DIR="$SCRIPT_DIR/.pids"

# Function to stop a single backend
stop_backend() {
    local port=$1
    local pid_file="$PID_DIR/backend_$port.pid"

    if [ ! -f "$pid_file" ]; then
        echo "Backend on port $port is not running (no PID file found)"
        return
    fi

    local pid=$(cat "$pid_file")

    if ps -p "$pid" > /dev/null 2>&1; then
        echo "Stopping backend server on port $port (PID: $pid)..."
        kill "$pid"

        # Wait for graceful shutdown (max 5 seconds)
        for i in {1..10}; do
            if ! ps -p "$pid" > /dev/null 2>&1; then
                echo "Backend on port $port stopped"
                rm -f "$pid_file"
                return
            fi
            sleep 0.5
        done

        # Force kill if still running
        if ps -p "$pid" > /dev/null 2>&1; then
            echo "Force stopping backend on port $port..."
            kill -9 "$pid"
            rm -f "$pid_file"
        fi
    else
        echo "Backend on port $port is not running (stale PID file)"
        rm -f "$pid_file"
    fi
}

# Function to stop all backends
stop_all() {
    echo "Stopping all backend servers..."
    for port in {8080..8089}; do
        stop_backend "$port"
    done
    echo "All backend servers stopped"
}

# Main script
if [ $# -eq 0 ]; then
    echo "Usage: $0 <port|all>"
    echo "  port: Stop backend on specific port (8080-8089)"
    echo "  all:  Stop all backend servers"
    exit 1
fi

if [ "$1" = "all" ]; then
    stop_all
else
    # Stop specific port
    port=$1
    if [[ "$port" =~ ^[0-9]+$ ]] && [ "$port" -ge 8080 ] && [ "$port" -le 8089 ]; then
        stop_backend "$port"
    else
        echo "Error: Invalid port $port. Must be between 8080-8089"
        exit 1
    fi
fi
