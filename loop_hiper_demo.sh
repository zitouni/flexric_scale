#!/bin/bash

# Default values
RUN_SECONDS=120
WAIT_SECONDS=60
CONFIG_PATH="../../../../Cu-36_Du-68_Du63.conf"

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -r) RUN_SECONDS=$2; shift 2 ;;
        -w) WAIT_SECONDS=$2; shift 2 ;;
        -c) CONFIG_PATH=$2; shift 2 ;;
        *) echo "Usage: $0 [-r run_seconds] [-w wait_seconds] [-c config_path]"; exit 1 ;;
    esac
done

# Check if config file exists
if [ ! -f "$CONFIG_PATH" ]; then
    echo "Error: Config file not found: $CONFIG_PATH"
    exit 1
fi

# Function to kill process and exit
cleanup() {
    echo -e "\nStopping all processes..."
    sudo pkill -f "hiper_ran_xapp"
    sudo pkill -f "restart_xapp.sh"
    exit 0
}

# Trap Ctrl+C
trap cleanup SIGINT

echo "Starting cycle (Run: ${RUN_SECONDS}s, Wait: ${WAIT_SECONDS}s)"
echo "Config: $CONFIG_PATH"
echo "Press Ctrl+C to stop"

# Main loop
while true; do
    echo "[$(date '+%H:%M:%S')] Starting xApp..."
    sudo ./hiper_ran_xapp -c "$CONFIG_PATH" &
    
    sleep $RUN_SECONDS
    echo "[$(date '+%H:%M:%S')] Stopping xApp..."
    sudo pkill -f "hiper_ran_xapp"
    sleep 2
    
    echo "[$(date '+%H:%M:%S')] Waiting ${WAIT_SECONDS}s..."
    sleep $WAIT_SECONDS
done
