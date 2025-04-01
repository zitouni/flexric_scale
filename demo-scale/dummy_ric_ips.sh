#!/bin/bash

# Array of interface names and corresponding IPs
declare -A IP_MAP=(
    ["nr-ric_1"]="192.168.130.200"
    ["nr-ric_2"]="192.168.130.201"
    ["nr-ric_3"]="192.168.130.202"
    ["nr-ric_4"]="192.168.130.203"
)

# Function to configure bandwidth for an interface
configure_bandwidth() {
    local interface=$1
    local bandwidth="1000mbit"
    
    # Delete existing qdisc if any
    sudo tc qdisc del dev "$interface" root 2>/dev/null
    
    # Add HTB qdisc
    sudo tc qdisc add dev "$interface" root handle 1: htb default 10
    
    # Add bandwidth class
    sudo tc class add dev "$interface" parent 1: classid 1:1 htb rate "$bandwidth" ceil "$bandwidth"
    
    echo "Configured bandwidth $bandwidth for interface $interface"
}

# Function to create interfaces
create_interfaces() {
    echo "Creating dummy interfaces..."
    # Load dummy module if not loaded
    sudo modprobe dummy
    
    # Create and configure each interface
    for interface in "${!IP_MAP[@]}"; do
        # Create dummy interface
        sudo ip link add "$interface" type dummy 2>/dev/null
        
        # Ensure interface is up
        sudo ip link set "$interface" up
        
        # Add IP address
        sudo ip addr add "${IP_MAP[$interface]}/24" dev "$interface" 2>/dev/null
        
        # Configure bandwidth
        configure_bandwidth "$interface"
        
        echo "Created interface $interface with IP: ${IP_MAP[$interface]}"
    done
    
    # Display current configuration
    echo -e "\nCurrent IP and bandwidth configuration:"
    for interface in "${!IP_MAP[@]}"; do
        echo -e "\nInterface: $interface"
        ip addr show "$interface"
        echo "Bandwidth configuration:"
        tc qdisc show dev "$interface"
        tc class show dev "$interface"
    done
}

# Function to delete interfaces
delete_interfaces() {
    echo "Deleting interfaces..."
    
    # Remove each interface
    for interface in "${!IP_MAP[@]}"; do
        # Remove tc configuration first
        sudo tc qdisc del dev "$interface" root 2>/dev/null
        # Delete interface
        sudo ip link delete "$interface" 2>/dev/null
        echo "Removed interface: $interface"
    done
}

# Function to show configuration
show_configuration() {
    echo -e "\nCurrent Configuration:"
    local found=0
    for interface in "${!IP_MAP[@]}"; do
        if ip addr show "$interface" &>/dev/null; then
            echo -e "\nInterface: $interface"
            ip addr show "$interface"
            echo -e "\nBandwidth configuration:"
            tc qdisc show dev "$interface"
            tc class show dev "$interface"
            found=1
        fi
    done
    
    if [ $found -eq 0 ]; then
        echo "No dummy interfaces exist"
    fi
}

# Function to show bandwidth configuration
show_bandwidth() {
    echo -e "\nBandwidth Configuration:"
    local found=0
    for interface in "${!IP_MAP[@]}"; do
        if ip link show "$interface" &>/dev/null; then
            echo -e "\nInterface: $interface"
            tc qdisc show dev "$interface"
            tc class show dev "$interface"
            found=1
        fi
    done
    
    if [ $found -eq 0 ]; then
        echo "No dummy interfaces exist"
    fi
}

# Main menu
while true; do
    echo -e "\nDummy Interface Manager"
    echo "1. Create interfaces"
    echo "2. Delete interfaces"
    echo "3. Show current configuration"
    echo "4. Show bandwidth configuration"
    echo "5. Exit"
    read -p "Select an option (1-5): " choice

    case $choice in
        1)
            create_interfaces
            ;;
        2)
            delete_interfaces
            ;;
        3)
            show_configuration
            ;;
        4)
            show_bandwidth
            ;;
        5)
            echo "Exiting..."
            exit 0
            ;;
        *)
            echo "Invalid option"
            ;;
    esac
done
