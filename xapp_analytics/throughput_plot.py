import sqlite3
import matplotlib.pyplot as plt
import numpy as np
from typing import Tuple, List, Dict
import os
from datetime import datetime

def connect_to_database(db_path: str) -> sqlite3.Connection:
    """Establish database connection"""
    try:
        return sqlite3.connect(db_path)
    except sqlite3.Error as e:
        print(f"Error connecting to database: {e}")
        return None

def create_output_directory(dir_name: str = "throughput_plot") -> str:
    """Create output directory if it doesn't exist"""
    current_dir = os.getcwd()
    output_dir = os.path.join(current_dir, dir_name)
    if not os.path.exists(output_dir):
        os.makedirs(output_dir)
    return output_dir


def fetch_data(conn: sqlite3.Connection, table: str, field: str) -> Tuple[List[float], List[float]]:
    """
    Fetch tstamp and field data from specified table
    
    Parameters:
    - conn: Database connection
    - table: Name of the table
    - field: Name of the field to fetch
    
    Returns:
    - Tuple of (timestamps in ns, values) as lists
    """
    try:
        cursor = conn.cursor()
        query = f"SELECT tstamp, {field} FROM {table} ORDER BY tstamp"
        cursor.execute(query)
        results = cursor.fetchall()
        if results:
            timestamps, values = zip(*results)
            return list(timestamps), list(values)
        return [], []
    except sqlite3.Error as e:
        print(f"Error fetching data from {table}.{field}: {e}")
        return [], []

def calculate_throughput(times: List[float], values: List[float], time_unit: str, is_bits: bool = False, field_name: str = "") -> Tuple[List[float], List[float]]:
    """
    Calculate throughput in Mbps from cumulative data using 1-second windows
    """
    if not times or not values:
        print("No data received for processing")
        return [], []
    
    print(f"Processing {len(times)} data points for field {field_name}...")
    
    # Convert to numpy arrays
    times_array = np.array(times, dtype=np.float64)
    values_array = np.array(values, dtype=np.float64)
    
    # Convert times to seconds and normalize to start from 0
    times_sec = (times_array - times_array[0]) / 1000000
    
    # Debug information
    print(f"Time range: {times_sec[0]:.2f}s to {times_sec[-1]:.2f}s")
    print(f"Value range: {values_array[0]} to {values_array[-1]}")
    
    # Calculate window-based throughput with 1-second windows
    window_size_us = 1000000  # 1 second in microseconds
    throughput_times = []
    throughput_values = []
    
    # Slide window through the data
    current_time = times_array[0]
    end_time = times_array[-1]
    
    # Ensure we process the entire time range
    while current_time <= end_time - window_size_us:
        window_end = current_time + window_size_us
        
        # Find data points within the current window
        window_mask = (times_array >= current_time) & (times_array < window_end)
        window_points = np.sum(window_mask)
        
        # Initialize throughput as 0 for this window
        throughput = 0
        
        if window_points >= 2:  # Need at least 2 points
            window_times = times_array[window_mask]
            window_values = values_array[window_mask]
            
            # Calculate time difference in seconds
            time_diff = (window_times[-1] - window_times[0]) / 1000000
            
            if time_diff > 0:  # Avoid division by zero
                if 'tbs' in field_name.lower():
                    # For TBS fields: calculate difference in bits
                    value_diff = window_values[-1] - window_values[0]
                    # Convert to Mbps (bits per second / 1e6)
                    if value_diff > 0:
                        # Normal case: positive increment
                        throughput = value_diff / (time_diff * 1e6)
                        last_throughput = throughput  # Store for next window
                    elif value_diff == 0:
                        # No change: maintain last throughput
                        throughput = last_throughput if 'last_throughput' in locals() else 0
                    else:
                        # Negative change: decrement from last throughput
                        #throughput = max(0, last_throughput - abs(value_diff) / (time_diff * 1e6)) if 'last_throughput' in locals() else 0
                        throughput =  window_values[-1] / (time_diff * 1e6)
                else:
                    # For byte fields: calculate difference in bytes
                    bytes_diff = window_values[-1] - window_values[0]
                    # Convert bytes to bits and then to Mbps
                    if bytes_diff > 0:
                        # Normal case: positive increment
                        throughput = (bytes_diff * 8) / (time_diff * 1e6)
                        last_throughput = throughput  # Store for next window
                    elif bytes_diff == 0:
                        # No change: maintain last throughput
                        throughput = last_throughput if 'last_throughput' in locals() else 0
                    else:
                        # Negative change: decrement from last throughput
                        throughput = (window_values[-1] * 8) / (time_diff * 1e6)

        # Always add the time point, even if throughput is 0
        plot_time = (current_time - times_array[0]) / 1000000
        throughput_times.append(plot_time)
        throughput_values.append(throughput)
        
        # Move to next second
        current_time += window_size_us
    
    # # Add final point to show complete time range
    # final_time = (end_time - times_array[0]) / 1000000
    # throughput_times.append(final_time)
    # throughput_values.append(0)  # Assume zero throughput at end
    
    if not throughput_times:
        print("No valid throughput values calculated")
        return [], []
    
    # Convert to numpy arrays for processing
    throughput_times = np.array(throughput_times)
    throughput_values = np.array(throughput_values)
    
    print(f"Generated {len(throughput_times)} throughput values")
    print(f"Throughput range: {np.min(throughput_values):.2f} to {np.max(throughput_values):.2f} Mbps")
    print(f"Time range covered: {throughput_times[0]:.2f}s to {throughput_times[-1]:.2f}s")
    
    if time_unit == 'minutes':
        # Convert to minute-based averages
        max_time = int(np.ceil(throughput_times[-1]))
        bins = np.arange(0, max_time + 60, 60)
        minute_indices = np.digitize(throughput_times, bins) - 1
        
        minute_averages = []
        minute_times = []
        
        for minute in range(len(bins)-1):
            minute_mask = minute_indices == minute
            if np.any(minute_mask):
                minute_averages.append(np.mean(throughput_values[minute_mask]))
                minute_times.append(minute)
        
        if not minute_averages:
            print("No minute-based averages calculated")
            return [], []
        
        return minute_times, minute_averages
    else:
        return throughput_times.tolist(), throughput_values.tolist()



def process_data(db_path: str, time_unit: str, title: str):
    """Process data and create visualization"""
    conn = connect_to_database(db_path)
    if not conn:
        return
    
    # Create output directory
    output_dir = create_output_directory()
    
    # Get table selection
    table_choice = get_table_selection(conn)
    if table_choice is None:
        conn.close()
        return
    
    # Get field selections
    selected_fields = get_field_selection(conn, table_choice)
    if not selected_fields:
        print("No fields selected.")
        conn.close()
        return
    
    data_dict = {}
    
    # Process selected fields
    for table, field, label in selected_fields:
        print(f"\nProcessing {table}.{field}...")
        times, values = fetch_data(conn, table, field)
        
        if not times or not values:
            print(f"No data found for {table}.{field}")
            continue
            
        print(f"Fetched {len(times)} records")
        
        # Determine if the field is in bits (MAC_UE) or bytes (PDCP_bearer)
        is_bits = 'tbs' in field.lower()  # Check if field contains 'tbs' for bit values
        print(f"Processing as {'bits' if is_bits else 'bytes'}")
        
        plot_times, throughput = calculate_throughput(
            times, 
            values, 
            time_unit, 
            is_bits
        )
        
        if plot_times and throughput:
            data_dict[label] = (plot_times, throughput)
            print(f"Successfully processed {label}")
        else:
            print(f"No valid data generated for {label}")
    
    conn.close()
    
    if data_dict:
        print(f"\nPlotting data for {len(data_dict)} series...")
        create_and_save_plot(data_dict, time_unit, title, output_dir)
    else:
        print("No data available to plot")

def create_and_save_plot(data_dict: Dict, time_unit: str, title: str, output_dir: str):
    """Create, configure, and save the plot with legend inside the grid"""
    plt.figure(figsize=(12, 6))
    
    # Find the maximum time across all data series
    max_time = 0
    for label, (times, values) in data_dict.items():
        if times and values:
            max_time = max(max_time, max(times))
    
    plot_created = False
    for label, (times, values) in data_dict.items():
        if times and values:
            # Clip values to 5 Mbps
            # clipped_values = np.clip(values, 0, 5)

            max_val = np.max(values)
            # # Add 10% margin to the maximum value
            # clip_threshold = max_val * 1.1
            # clipped_values = np.clip(values, 0, clip_threshold)
            
            # Split the label into table name and field name
            parts = label.split()
            if len(parts) >= 2:
                table_name = parts[0]
                field_name = parts[1]
                
                # Modify the label based on whether it contains 'ul' or 'dl'
                if "ul" in label.lower():
                    display_label = f"UL Throughput ({table_name}/{field_name})"
                elif "dl" in label.lower():
                    display_label = f"DL Throughput ({table_name}/{field_name})"
                else:
                    display_label = label
            else:
                # Add "CU/" prefix if the table is PDCP_bearer
                if table_name == "PDCP_bearer":
                    table_name = f"CU/{table_name}"
                    
                display_label = label  # Keep original label if it can't be split
            
            if time_unit == 'minutes':
                plt.plot(times, values, label=display_label, alpha=0.7, 
                        marker='o', markersize=4, linestyle='-', linewidth=1)
            else:
                plt.plot(times, values, label=display_label, alpha=0.7)
            plot_created = True

    if not plot_created:
        print("No valid data to plot")
        plt.close()
        return
    
    plt.grid(True, alpha=0.3)
    plt.xlabel(f'Time ({time_unit})')
    plt.ylabel('Throughput (Megabits/s)')
    plt.title(title)
    
    # Set x-axis limits
    plt.xlim(left=0)
    if time_unit == 'minutes':
        # Round up to next minute
        max_minutes = int(np.ceil(max_time))
        plt.xlim(right=max_minutes)
        
        # Set x-ticks to show every minute
        plt.xticks(np.arange(0, max_minutes + 1, 1))
    
    # Set fixed y-axis limits (0-5 Mbps)
    plt.ylim(0, max_val+1)
    
    # Create custom y-ticks (0 to 5 Mbps)
    yticks = np.linspace(0, max_val, 11)  # 11 ticks for 0 to 5 with 0.5 step
    plt.yticks(yticks)
    
    # Format y-axis ticks to show values with one decimal place
    def format_throughput(x, p):
        return f'{x:.1f}'
    
    plt.gca().yaxis.set_major_formatter(plt.FuncFormatter(format_throughput))
    
    # Add minor gridlines
    plt.grid(True, which='minor', alpha=0.15)
    plt.minorticks_on()
    
    # Place legend inside the plot in the upper right corner
    # bbox_to_anchor can be adjusted to fine-tune position
    plt.legend(loc='best', 
              #bbox_to_anchor=(0.99, 0.99),
              fancybox=True, 
              framealpha=0.8,  # Semi-transparent background
              edgecolor='gray')  # Gray edge for better visibility
    
    #plt.legend.set_draggable(True) # Makes the legend draggable with mouse
    
    plt.tight_layout()
    
    # Generate filename with timestamp
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    # Replace any spaces in title with underscores and remove special characters
    safe_title = "".join(c if c.isalnum() or c in ('-', '_') else '_' for c in title.replace(' ', '_'))
    filename = f"{safe_title}_{timestamp}.png"
    filepath = os.path.join(output_dir, filename)
    
    # Save plot with high DPI
    plt.savefig(filepath, dpi=300, bbox_inches='tight')
    print(f"Plot saved as: {filepath}")
    
    # Display plot
    plt.show()

def get_table_list(conn: sqlite3.Connection) -> List[str]:
    """Get list of tables from database"""
    try:
        cursor = conn.cursor()
        cursor.execute("SELECT name FROM sqlite_master WHERE type='table'")
        tables = cursor.fetchall()
        return [table[0] for table in tables]
    except sqlite3.Error as e:
        print(f"Error getting tables: {e}")
        return []

def get_field_list(conn: sqlite3.Connection, table: str) -> List[str]:
    """Get list of fields from specified table"""
    try:
        cursor = conn.cursor()
        cursor.execute(f"PRAGMA table_info({table})")
        fields = cursor.fetchall()
        # Return all field names including 'tstamp'
        return [field[1] for field in fields]
    except sqlite3.Error as e:
        print(f"Error getting fields from {table}: {e}")
        return []

def get_field_selection(conn: sqlite3.Connection, table: str) -> list:
    """Get user selection for fields"""
    fields = get_field_list(conn, table)
    if not fields:
        print(f"No fields found in table {table}.")
        return []
    
    print(f"\nAvailable fields in {table}:")
    for i, field in enumerate(fields, 1):
        print(f"{i}. {field}")
    print("\nEnter field number to select, or 'f' to finish selection")
    
    selected_fields = []
    while True:
        try:
            choice = input(f"Select field (or 'f' to finish): ").strip().lower()
            if choice == 'f':
                break
            try:
                choice = int(choice)
                if 1 <= choice <= len(fields):
                    field_name = fields[choice-1]
                    if field_name == 'tstamp':
                        print("Cannot select 'tstamp' field for plotting.")
                        continue
                    # Create a label based on the table and field name
                    label = f"{table} {field_name}"
                    if (table, field_name, label) not in selected_fields:
                        selected_fields.append((table, field_name, label))
                        print(f"Selected: {label}")
                    else:
                        print("Field already selected.")
                else:
                    print(f"Invalid choice. Please enter a number between 1 and {len(fields)}")
            except ValueError:
                print("Invalid input. Please enter a number or 'f' to finish.")
        except KeyboardInterrupt:
            print("\nOperation cancelled by user.")
            return []
    
    return selected_fields


def get_table_selection(conn: sqlite3.Connection) -> str:
    """Get user selection for table"""
    tables = get_table_list(conn)
    if not tables:
        print("No tables found in database.")
        return None
    
    print("\nAvailable tables:")
    for i, table in enumerate(tables, 1):
        print(f"{i}. {table}")
    
    while True:
        try:
            choice = int(input("Select table number: "))
            if 1 <= choice <= len(tables):
                return tables[choice-1]
            else:
                print(f"Invalid choice. Please enter a number between 1 and {len(tables)}")
        except ValueError:
            print("Invalid input. Please enter a number.")
        except KeyboardInterrupt:
            print("\nOperation cancelled by user.")
            return None


def main():
    try:
        while True:
            # Get user input
            db_path = input("Enter the path to the SQLite database (or 'quit' to exit): ").strip()
            if db_path.lower() == 'quit':
                break
                
            if not os.path.exists(db_path):
                print("Database file not found.")
                continue
                
            # Modified time unit selection using numbers
            print("\nSelect time unit:")
            print("1. seconds")
            print("2. minutes")
            
            while True:
                try:
                    time_choice = int(input("Enter your choice (1 or 2): "))
                    if time_choice in [1, 2]:
                        time_unit = 'seconds' if time_choice == 1 else 'minutes'
                        break
                    else:
                        print("Invalid choice. Please enter 1 or 2.")
                except ValueError:
                    print("Invalid input. Please enter a number (1 or 2).")
                except KeyboardInterrupt:
                    print("\nOperation cancelled by user.")
                    return
                
            title = input("\nEnter plot title (press Enter for default): ").strip()
            if not title:
                title = "Throughput Over Time"
                
            process_data(db_path, time_unit, title)

    except KeyboardInterrupt:
        print("\nProgram terminated by user.")
    except Exception as e:
        print(f"\nAn error occurred: {e}")
    finally:
        print("\nExiting program.")


if __name__ == "__main__":
    main()

