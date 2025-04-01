#!/bin/bash

# Default values
RUN_SECONDS=230
WAIT_SECONDS=35
CONFIG_PATH="Cu-36_Du-68_Du63.conf"
XAPP_PATH="build/examples/xApp/ics/hiper_ran_xapp"
RIC_PATH="build/examples/ric/nearRT-RIC"

# Check if we're in the flexric directory
if [ ! -d "build/examples/xApp/ics" ] || [ ! -d "build/examples/ric" ]; then
    echo "Error: Script must be run from the flexric directory"
    echo "Current directory: $(pwd)"
    echo "Expected path to xApp: $(pwd)/${XAPP_PATH}"
    echo "Expected path to RIC: $(pwd)/${RIC_PATH}"
    exit 1
fi

# Check if executables exist
if [ ! -f "${XAPP_PATH}" ]; then
    echo "Error: hiper_ran_xapp not found at: ${XAPP_PATH}"
    echo "Please ensure the application is built correctly"
    exit 1
fi

if [ ! -f "${RIC_PATH}" ]; then
    echo "Error: nearRT-RIC not found at: ${RIC_PATH}"
    echo "Please ensure the application is built correctly"
    exit 1
fi

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
    echo "Current directory: $(pwd)"
    echo "Full config path: $(realpath ${CONFIG_PATH})"
    exit 1
fi

# Function to kill process and exit
cleanupAll() {
    echo -e "\nStopping all processes..."
    sudo pkill -f "hiper_ran_xapp"
    sudo pkill -f "nearRT-RIC"
    sudo pkill -f "loop_hiper_demo.sh"
    exit 0
}

# Trap Ctrl+C
trap cleanupAll SIGINT

echo "Starting cycle (Run: ${RUN_SECONDS}s, Wait: ${WAIT_SECONDS}s)"
echo "Config: $CONFIG_PATH"
echo "xApp path: ${XAPP_PATH}"
echo "RIC path: ${RIC_PATH}"
echo "Press Ctrl+C to stop"

# Main loop
while true; do
    echo "[$(date '+%H:%M:%S')] Starting nearRT-RIC..."
    sudo ./${RIC_PATH} -c "$CONFIG_PATH" &
    
    # Wait 3 seconds before starting xApp
    sleep 3
    
    echo "[$(date '+%H:%M:%S')] Starting xApp..."
    sudo ./${XAPP_PATH} -c "$CONFIG_PATH" &
    
    sleep $RUN_SECONDS
    echo "[$(date '+%H:%M:%S')] Stopping processes..."
    sudo pkill -f "hiper_ran_xapp"
    sudo pkill -f "nearRT-RIC"
    sleep 2
    echo "[$(date '+%H:%M:%S')] Waiting ${WAIT_SECONDS}s..."
    sleep $WAIT_SECONDS
done
