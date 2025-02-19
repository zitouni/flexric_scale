import sqlite3
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
from typing import List, Dict, Tuple
import os
import json
from datetime import datetime

def get_tables_from_db(db_path: str) -> List[str]:
    """Get all tables from a database"""
    try:
        conn = sqlite3.connect(db_path)
        cursor = conn.cursor()
        cursor.execute("SELECT name FROM sqlite_master WHERE type='table';")
        tables = [table[0] for table in cursor.fetchall()]
        conn.close()
        return tables
    except sqlite3.Error as e:
        print(f"Error accessing database: {e}")
        return []

def get_table_fields(db_path: str, table_name: str) -> List[str]:
    """Get field names from a table"""
    try:
        conn = sqlite3.connect(db_path)
        cursor = conn.cursor()
        cursor.execute(f"PRAGMA table_info({table_name})")
        fields = [field[1] for field in cursor.fetchall()]
        conn.close()
        return fields
    except sqlite3.Error as e:
        print(f"Error getting fields: {e}")
        return []

def fetch_data(db_path: str, table: str, field: str) -> Tuple[np.ndarray, np.ndarray]:
    """Fetch timestamp and field data from database"""
    try:
        conn = sqlite3.connect(db_path)
        query = f"SELECT tstamp, {field} FROM {table} WHERE {field} IS NOT NULL ORDER BY tstamp"
        df = pd.read_sql_query(query, conn)
        conn.close()
        
        # Convert to numeric timestamps if not already
        timestamps = pd.to_numeric(df['tstamp'])
        # Convert nanoseconds to seconds
        timestamps = timestamps / 1000000
        # Normalize timestamps to start from 0
        timestamps = timestamps - timestamps.min()
        
        return timestamps.to_numpy(), df[field].to_numpy()
    except Exception as e:
        print(f"Error fetching data: {e}")
        return np.array([]), np.array([])
    
def format_bitrate(value: float) -> Tuple[float, str]:
    """
    Convert bits to appropriate unit (Kb, Mb, Gb)
    Returns tuple of (scaled_value, unit_string)
    """
    if value >= 1e9:
        return value/1e9, 'Gb'
    elif value >= 1e6:
        return value/1e6, 'Mb'
    elif value >= 1e3:
        return value/1e3, 'Kb'
    else:
        return value, 'bits'

def get_field_unit_and_type(field_name: str) -> Tuple[str, str]:
    """Determine the unit and type based on field name"""
    throughput_fields = ['dl_aggr_tbs', 'ul_aggr_tbs', 'dl_curr_tbs', 'ul_curr_tbs']
    if field_name in throughput_fields:
        return 'bits', 'Throughput'
    return '', ''

def scale_values(values: np.ndarray, field_name: str) -> Tuple[np.ndarray, str, str]:
    """Scale values and return appropriate unit label and type"""
    unit, value_type = get_field_unit_and_type(field_name)
    
    if unit == 'bits':
        # Find the appropriate scale for the maximum value
        max_value = np.max(values)
        _, unit = format_bitrate(max_value)
        
        # Scale all values accordingly
        if unit == 'Gb':
            return values/1e9, unit, value_type
        elif unit == 'Mb':
            return values/1e6, unit, value_type
        elif unit == 'Kb':
            return values/1e3, unit, value_type
        return values, 'bits', value_type
    return values, '', value_type

def save_plot(fig, data_sources: List[Dict], plot_title: str, plot_cdf: bool, log_scale: str):
    """
    Save plot with organized naming convention and folder structure
    
    Parameters:
    - fig: matplotlib figure object
    - data_sources: List of dictionaries containing plot data information
    - plot_title: Title of the plot
    - plot_cdf: Boolean indicating if CDF is included
    - log_scale: String indicating log scale option
    """
    # Create base directory for results
    save_dir = "comparison_results"
    timestamp = datetime.now().strftime("%Y%m%d-%H%M%S")
    
    # Create timestamp-based subdirectory
    plot_dir = os.path.join(save_dir, timestamp)
    os.makedirs(plot_dir, exist_ok=True)
    
    # Create filename components
    fields_str = "_".join(sorted([source['field'] for source in data_sources]))
    if len(fields_str) > 50:  # Truncate if too long
        fields_str = fields_str[:47] + "..."
    
    # Add plot characteristics to filename
    plot_type = "with_CDF" if plot_cdf else "no_CDF"
    scale_type = f"scale_{log_scale}"
    
    # Create base filename
    if plot_title:
        # Clean plot title for filename
        clean_title = "".join(c if c.isalnum() else "_" for c in plot_title)
        clean_title = clean_title[:50]  # Limit length
        base_name = f"{clean_title}_{fields_str}"
    else:
        base_name = fields_str
    
    # Construct final filename
    filename = f"{base_name}_{plot_type}_{scale_type}.png"
    filepath = os.path.join(plot_dir, filename)
    
    # Save plot
    fig.savefig(filepath, bbox_inches='tight', dpi=300)
    
    # Create metadata file
    metadata = {
        "timestamp": timestamp,
        "plot_title": plot_title,
        "data_sources": [
            {
                "database": os.path.basename(source['db']),
                "table": source['table'],
                "field": source['field'],
                "legend": source['legend']
            }
            for source in data_sources
        ],
        "plot_settings": {
            "cdf_included": plot_cdf,
            "scale_type": log_scale
        }
    }
    
    # Save metadata
    metadata_file = os.path.join(plot_dir, "plot_metadata.json")
    with open(metadata_file, "w") as f:
        json.dump(metadata, f, indent=4)
    
    print(f"\nPlot saved as: {filepath}")
    print(f"Metadata saved as: {metadata_file}")

def save_plot_and_metadata(fig, data_sources: List[Dict], plot_title: str, plot_cdf: bool, 
                          log_scale: str, time_unit: str) -> Tuple[str, str]:
    """
    Save plot and its metadata to a timestamped directory
    
    Parameters:
    - fig: matplotlib figure object
    - data_sources: List of dictionaries containing plot data information
    - plot_title: Title of the plot
    - plot_cdf: Boolean indicating if CDF is included
    - log_scale: String indicating log scale option
    - time_unit: String indicating time unit used ('minutes' or 'seconds')
    
    Returns:
    - Tuple[str, str]: Paths to saved plot and metadata files
    """
    # Create timestamped directory
    timestamp = datetime.now().strftime("%Y%m%d-%H%M%S")
    save_dir = os.path.join("comparison_results", timestamp)
    os.makedirs(save_dir, exist_ok=True)
    
    # Create fields string
    fields_str = "_".join(sorted([source['field'] for source in data_sources]))
    if len(fields_str) > 50:
        fields_str = fields_str[:47] + "..."
    
    # Create filename components
    plot_type = "with_CDF" if plot_cdf else "no_CDF"
    scale_type = f"scale_{log_scale}"
    time_type = f"time_{time_unit}"
    
    # Create base name
    if plot_title:
        clean_title = "".join(c if c.isalnum() else "_" for c in plot_title)
        clean_title = clean_title[:50]
        base_name = f"{clean_title}_{fields_str}"
    else:
        base_name = fields_str
    
    # Create full filename
    filename = f"{base_name}_{plot_type}_{scale_type}_{time_type}.png"
    filepath = os.path.join(save_dir, filename)
    
    # Save plot with optimized settings
    fig.savefig(filepath, bbox_inches='tight', dpi=300)
    
    # Create and save metadata
    metadata = {
        "timestamp": timestamp,
        "plot_title": plot_title,
        "data_sources": [
            {
                "database": os.path.basename(source['db']),
                "table": source['table'],
                "field": source['field'],
                "legend": source['legend']
            }
            for source in data_sources
        ],
        "plot_settings": {
            "cdf_included": plot_cdf,
            "scale_type": log_scale,
            "time_unit": time_unit,
            "averaging": True if time_unit == 'minutes' else False
        }
    }
    
    metadata_file = os.path.join(save_dir, "plot_metadata.json")
    with open(metadata_file, "w") as f:
        json.dump(metadata, f, indent=4)
    
    print(f"\nPlot saved as: {filepath}")
    print(f"Metadata saved as: {metadata_file}")
    
    return filepath, metadata_file

def calculate_throughput(db_path: str, direction: str) -> Tuple[np.ndarray, np.ndarray]:
    """
    Calculate throughput from accumulated bytes in PDCP_bearer table
    
    Parameters:
    - db_path: Path to the SQLite database
    - direction: String indicating direction ('tx' or 'rx')
    
    Returns:
    - Tuple[np.ndarray, np.ndarray]: timestamps and throughput values in bits/s
    """
    try:
        conn = sqlite3.connect(db_path)
        
        # Select appropriate column based on direction
        bytes_column = 'txpdu_bytes' if direction == 'tx' else 'rxpdu_bytes'
        query = f"""
            SELECT tstamp, {bytes_column}
            FROM PDCP_bearer
            WHERE {bytes_column} IS NOT NULL
            ORDER BY tstamp
        """
        
        df = pd.read_sql_query(query, conn)
        conn.close()
        
        if df.empty:
            print(f"No data found for {bytes_column}")
            return np.array([]), np.array([])
        
        # Convert timestamps to numeric if not already
        timestamps = pd.to_numeric(df['tstamp'])
        bytes_values = df[bytes_column]
        
        # Calculate time differences in seconds
        time_diff = np.diff(timestamps) / 1000000  # Convert ns to seconds
        
        # Calculate byte differences
        bytes_diff = np.diff(bytes_values)
        
        # Calculate throughput in bits/s
        # (bytes_diff * 8) for bits, then / 1_000_000 for Mega
        throughput = (bytes_diff * 8) / (time_diff * 1_000_000)
        
        # Use timestamps from second point onwards (since we lose one point in diff)
        timestamps = timestamps[1:]
        
        # Remove any negative throughput values (if any) that might occur due to counter reset
        valid_mask = throughput >= 0
        timestamps = timestamps[valid_mask]
        throughput = throughput[valid_mask]
        
        return timestamps, throughput
        
    except Exception as e:
        print(f"Error calculating throughput: {e}")
        return np.array([]), np.array([])

def get_field_unit(field_name: str) -> str:
    """
    Determine the unit based on the field name.
    
    Args:
        field_name (str): Name of the field from the database
        
    Returns:
        str: The appropriate unit for the field or empty string if no specific unit
    """
    field_name = field_name.lower()
    if 'rsrp' in field_name:
        return 'dBm'
    elif 'snr' in field_name:
        return 'dB'
    elif 'cqi' in field_name:
        return ''  # No unit for CQI
    return ''  # Default case for other fields


def plot_comparison(data_sources: List[Dict], plot_cdf: bool = False, log_scale: str = 'none', 
                   plot_title: str = None, time_unit: str = 'minutes'):
    """
    Plot multiple data series with optional CDF and logarithmic scaling
    
    Parameters:
    - data_sources: List of dictionaries containing db, table, and field info
    - plot_cdf: Boolean to include CDF plot
    - log_scale: String indicating log scale option ('none', 'y', 'x', 'both')
    - plot_title: Custom title for the plot (optional)
    - time_unit: String indicating time unit ('minutes' or 'seconds')
    """
    # Optimize plot rendering
    plt.rcParams['path.simplify'] = True
    plt.rcParams['path.simplify_threshold'] = 1.0
    plt.rcParams['agg.path.chunksize'] = 10000
    plt.rcParams['figure.dpi'] = 100

    # Create figure with subplots
    if plot_cdf:
        fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(12, 12))
    else:
        fig, ax1 = plt.subplots(1, 1, figsize=(12, 8))
    
    # Group fields by their units and types for y-axis label
    field_groups = {}

    # Plot time series
    # Plot time series
    for source in data_sources:
        # Check if this is a PDCP throughput calculation
        if source['table'] == 'PDCP_bearer' and source['field'] in ['txpdu_bytes', 'rxpdu_bytes']:
            direction = 'tx' if source['field'] == 'txpdu_bytes' else 'rx'
            times, values = calculate_throughput(source['db'], direction)
            if len(times) == 0:
                print(f"Warning: No throughput data available for {direction.upper()}")
                continue
            # Values are already in Mbps, no need for additional scaling
            unit = 'Mbps'
            value_type = 'Throughput'
            scaled_values = values  # Already in Mbps
        else:
            # Regular data fetching for other fields
            times, values = fetch_data(source['db'], source['table'], source['field'])
            
        if len(times) > 0:
            # Convert to numpy arrays for faster processing
            times = np.array(times)
            values = np.array(values)
            
            # Filter zero values for specified fields before scaling
            if source['field'].lower() in ['rsrp', 'cqi'] or \
               (source['table'] == 'MAC_UE' and source['field'] in ['rxpdu_bytes', 'txpdu_bytes']) or \
               (source['table'] == 'PDCP_bearer' and source['field'] in ['dl_aggr_tbs', 'ul_aggr_tbs']):
                non_zero_mask = values != 0
                times = times[non_zero_mask]
                values = values[non_zero_mask]
                
                if len(values) == 0:
                    print(f"Warning: No non-zero values found for {source['field']} in {source['table']}")
                    continue

            # Time conversion based on selected unit
            start_time = times[0]
            if time_unit == 'minutes':
                times_normalized = (times - start_time) / 60  # Convert to minutes
                
                # Calculate averages per minute using numpy's histogram
                bin_edges = np.arange(np.floor(times_normalized.min()), 
                                    np.ceil(times_normalized.max()) + 1)
                
                counts, _ = np.histogram(times_normalized, bins=bin_edges)
                sums, _ = np.histogram(times_normalized, bins=bin_edges, weights=values)
                
                # Avoid division by zero and compute averages
                valid_bins = counts > 0
                plot_values = np.zeros_like(counts, dtype=float)
                plot_values[valid_bins] = sums[valid_bins] / counts[valid_bins]
                
                # Calculate bin centers for x-axis
                plot_times = bin_edges[:-1] + 0.5
                
                # Only keep non-empty bins
                plot_times = plot_times[valid_bins]
                plot_values = plot_values[valid_bins]
                
            else:  # seconds
                # Convert timestamps to seconds and average within 1-second intervals
                times_normalized = (times - start_time)  # Keep in seconds
                
                # Calculate averages per second using numpy's histogram
                bin_edges = np.arange(np.floor(times_normalized.min()), 
                                    np.ceil(times_normalized.max()) + 1)
                
                counts, _ = np.histogram(times_normalized, bins=bin_edges)
                sums, _ = np.histogram(times_normalized, bins=bin_edges, weights=values)
                
                # Avoid division by zero and compute averages
                valid_bins = counts > 0
                plot_values = np.zeros_like(counts, dtype=float)
                plot_values[valid_bins] = sums[valid_bins] / counts[valid_bins]
                
                # Calculate bin centers for x-axis
                plot_times = bin_edges[:-1] + 0.5
                
                # Only keep non-empty bins
                plot_times = plot_times[valid_bins]
                plot_values = plot_values[valid_bins]
                
            # Scale the values (skip for PDCP throughput as it's already in bits/s)
            if source['table'] == 'PDCP_bearer' and source['field'] in ['txpdu_bytes', 'rxpdu_bytes']:
                scaled_values = plot_values
                unit = 'bits/s'
                value_type = 'Throughput'
            else:
                scaled_values, unit, value_type = scale_values(plot_values, source['field'])
            
            # Create group key combining unit and type
            group_key = (unit, value_type)
            if group_key not in field_groups:
                field_groups[group_key] = []
            
            # Add appropriate field name to groups
            field_groups[group_key].append(source['field'])
            label = f"{source['legend']} ({source['field']}) "  # Add unit to label
            
            # Plot with appropriate style based on time unit
            if time_unit == 'minutes':
                # Plot averaged data with markers
                ax1.plot(plot_times, scaled_values, label=label, alpha=0.7,
                        marker='o', markersize=4, linestyle='-', linewidth=1)
            else:
                # Plot raw data with smaller or no markers due to higher density
                ax1.plot(plot_times, scaled_values, label=label, alpha=0.7,
                        linewidth=1)
            
            # Plot CDF if requested
            if plot_cdf:
                sorted_data = np.sort(scaled_values)
                cumulative = np.linspace(1/len(sorted_data), 1, len(sorted_data))
                ax2.plot(sorted_data, cumulative, label=label, alpha=0.7)

    # Reduce the number of ticks if there are too many points
    if time_unit == 'minutes' and len(plot_times) > 20:
        ax1.xaxis.set_major_locator(plt.MaxNLocator(20))

    # Create y-axis label combining all units and types
    y_label_parts = []
    for (unit, value_type), fields in field_groups.items():
        if unit:
            if value_type:
                y_label_parts.append(f"{value_type} ({unit})")
            else:
                y_label_parts.append(f"{' / '.join(sorted(set(fields)))} ({unit})")
        else:
            if value_type:
                y_label_parts.append(f"{value_type}")
            else:
                y_label_parts.append(' / '.join(sorted(set(fields))))
    y_label = '\n'.join(y_label_parts)
    
        # Configure time series plot
    ax1.set_xlabel(f'Time ({time_unit})')
    
    # Set y-axis label based on field groups
    if field_groups:  # Check if field_groups is not empty
        if len(field_groups) == 1:
            fields = next(iter(field_groups.values()))  # Get the fields for this group
            field_name = next(iter(fields))  # Get the first (and presumably only) field name
            unit = get_field_unit(field_name)
            if unit:
                ax1.set_ylabel(f'{field_name} ({unit})')
            else:
                ax1.set_ylabel(field_name)
        else:
            # Use list comprehension for multiple fields
            y_label_parts = []
            for (_, _), fields in field_groups.items():
                field_name = next(iter(fields))
                unit = get_field_unit(field_name)
                if unit:
                    y_label_parts.append(f'{field_name} ({unit})')
                else:
                    y_label_parts.append(field_name)
            ax1.set_ylabel('\n'.join(y_label_parts))


    # Configure basic plot settings
    ax1.set_title(plot_title or 'Time Series Comparison')
    ax1.grid(True, alpha=0.3)
    ax1.legend(loc='best', bbox_to_anchor=None, framealpha=0.9)
    
    # Apply logarithmic scaling if requested
    if log_scale in ('y', 'both'):
        ax1.set_yscale('log')
    if log_scale in ('x', 'both'):
        ax1.set_xscale('log')
    
    # Configure CDF plot if requested
    if plot_cdf:
        # Set CDF x-label based on data type
        field_groups_keys = list(field_groups.keys())  # Cache keys to avoid multiple calls
        is_throughput = any(vtype == 'Throughput' for _, vtype in field_groups_keys)
        
        if is_throughput:
            throughput_unit = next((unit for unit, vtype in field_groups_keys if vtype == 'Throughput'), '')
            ax2.set_xlabel(f'Throughput ({throughput_unit})')
        else:
            ax2.set_xlabel('Values')

        # Configure CDF plot settings
        ax2.set_ylabel('Cumulative Probability')
        ax2.set_title('Cumulative Distribution Function')
        ax2.grid(True, alpha=0.3)
        ax2.legend(loc='best', bbox_to_anchor=None, framealpha=0.9)
        
        # Apply x-axis log scale if requested
        if log_scale in ('x', 'both'):
            ax2.set_xscale('log')
    
    plt.tight_layout()
    
    # Save plot and return
    return save_plot_and_metadata(
        fig, 
        data_sources, 
        plot_title, 
        plot_cdf, 
        log_scale, 
        time_unit
    ), plt.show()



def main():
    # Create base results directory
    os.makedirs("comparison_results", exist_ok=True)
    
    while True:
        data_sources = []
        
        while True:
            # Get database path
            db_path = input("\nEnter database path (or 'done' to finish adding sources): ").strip()
            if db_path.lower() == 'done':
                break
                
            if not os.path.exists(db_path):
                print("Database file not found.")
                continue
                
            # Get tables
            tables = get_tables_from_db(db_path)
            if not tables:
                continue
                
            print("\nAvailable tables:")
            for idx, table in enumerate(tables, 1):
                print(f"{idx}. {table}")
                
            # Select table
            while True:
                try:
                    table_idx = int(input("Select table number: ")) - 1
                    if 0 <= table_idx < len(tables):
                        selected_table = tables[table_idx]
                        break
                    print("Invalid table number.")
                except ValueError:
                    print("Please enter a valid number.")
                    
            # Get fields
            fields = get_table_fields(db_path, selected_table)
            if not fields:
                continue
                
            print("\nAvailable fields:")
            for idx, field in enumerate(fields, 1):
                print(f"{idx}. {field}")
                
            # Select field
            while True:
                try:
                    field_idx = int(input("Select field number: ")) - 1
                    if 0 <= field_idx < len(fields):
                        selected_field = fields[field_idx]
                        break
                    print("Invalid field number.")
                except ValueError:
                    print("Please enter a valid number.")
            
            # Get legend label
            legend_name = input("\nEnter a name for this data series (press Enter for default): ").strip()
            if not legend_name:
                legend_name = f"{os.path.basename(db_path)}:{selected_table}"
            
            # Add to data sources
            data_sources.append({
                'db': db_path,
                'table': selected_table,
                'field': selected_field,
                'legend': legend_name
            })
        
        if data_sources:
            # Get plot title
            plot_title = input("\nEnter the title for the plot (press Enter for default): ").strip()
            
            # Ask about time unit
            print("\nSelect time unit for x-axis:")
            print("1. Minutes (with averaging)")
            print("2. Seconds (raw data)")
            while True:
                try:
                    time_choice = input("Select option (1-2): ").strip()
                    if time_choice in ['1', '2']:
                        time_unit = 'minutes' if time_choice == '1' else 'seconds'
                        break
                    print("Invalid option. Please enter 1 or 2.")
                except ValueError:
                    print("Please enter a valid number.")
            
            # Ask about CDF
            plot_cdf = input("\nDo you want to include CDF plots? (y/n): ").lower().startswith('y')
            
            # Ask about logarithmic scaling
            print("\nSelect logarithmic scaling option:")
            print("1. No logarithmic scaling")
            print("2. Logarithmic Y-axis")
            print("3. Logarithmic X-axis")
            print("4. Logarithmic both axes")
            
            while True:
                try:
                    log_option = int(input("Select option (1-4): "))
                    if 1 <= log_option <= 4:
                        break
                    print("Invalid option.")
                except ValueError:
                    print("Please enter a valid number.")
            
            log_scale_map = {
                1: 'none',
                2: 'y',
                3: 'x',
                4: 'both'
            }
            log_scale = log_scale_map[log_option]
            
            # Generate and save plots
            plot_comparison(data_sources, plot_cdf, log_scale, plot_title, time_unit)
            
            # Ask if user wants to create another plot
            if input("\nDo you want to create another plot? (y/n): ").lower().strip() != 'y':
                break
        else:
            print("No data sources were added.")
            break

if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\nOperation cancelled by user.")
    except Exception as e:
        print(f"\nAn error occurred: {str(e)}")


