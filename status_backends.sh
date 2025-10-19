#!/bin/bash

# Script to check status of backend servers

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PID_DIR="$SCRIPT_DIR/.pids"

echo "Backend Server Status:"
echo "======================"

running_count=0
stopped_count=0

for port in {8080..8089}; do
    pid_file="$PID_DIR/backend_$port.pid"

    if [ -f "$pid_file" ]; then
        pid=$(cat "$pid_file")
        if ps -p "$pid" > /dev/null 2>&1; then
            echo "Port $port: RUNNING (PID: $pid)"
            ((running_count++))
        else
            echo "Port $port: STOPPED (stale PID file)"
            ((stopped_count++))
        fi
    else
        echo "Port $port: STOPPED"
        ((stopped_count++))
    fi
done

echo "======================"
echo "Running: $running_count | Stopped: $stopped_count"
