import sqlite3
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
from typing import List, Dict
import os
import json
from datetime import datetime

def normalize_data(data: np.ndarray) -> np.ndarray:
    """Normalize data to range [0,1]"""
    min_val = np.min(data)
    max_val = np.max(data)
    if max_val == min_val:
        return np.zeros_like(data)
    return (data - min_val) / (max_val - min_val)

def filter_zero_values(df: pd.DataFrame) -> pd.DataFrame:
    """Filter out rows where rsrp, cqi, ul_curr_tbs, or pusch_snr are zero"""
    return df[
        (df['rsrp'] != 0) & 
        (df['cqi'] != 0) & 
        (df['ul_curr_tbs'] != 0) &
        (df['pusch_snr'] != 0)
    ]

def fetch_mac_ue_data(db_path: str) -> tuple:
    """Fetch and normalize RSRP, CQI, UL_CURR_TBS, and PUSCH_SNR data"""
    try:
        # Connect with read-only mode and increased timeout
        conn = sqlite3.connect(f"file:{db_path}?mode=ro", uri=True, timeout=60)
        
        # First, get the count of rows to process
        cursor = conn.cursor()
        cursor.execute("SELECT COUNT(*) FROM MAC_UE")
        total_rows = cursor.fetchone()[0]
        print(f"Total rows in database: {total_rows:,}")

        # Process data in chunks
        chunk_size = 100000  # Adjust this value based on your available memory
        chunks = []
        
        print("\nLoading data in chunks...")
        
        # Calculate number of chunks
        num_chunks = (total_rows + chunk_size - 1) // chunk_size
        
        for i, chunk in enumerate(pd.read_sql_query(
            """
            SELECT tstamp, rsrp, cqi, ul_curr_tbs, pusch_snr
            FROM MAC_UE 
            WHERE rsrp IS NOT NULL 
                AND cqi IS NOT NULL 
                AND ul_curr_tbs IS NOT NULL 
                AND pusch_snr IS NOT NULL
            ORDER BY tstamp
            """, 
            conn, 
            chunksize=chunk_size
        )):
            print(f"\rProcessing chunk {i+1}/{num_chunks} ({((i+1)/num_chunks)*100:.1f}%)", end="")
            chunks.append(chunk)
        
        print("\nConcatenating chunks...")
        df = pd.concat(chunks, ignore_index=True)
        conn.close()
        
        print("Filtering zero values...")
        df = filter_zero_values(df)

        if df.empty:
            print("No valid data after filtering zero values")
            return None, None, None, None, None, None

        print("Processing timestamps...")
        timestamps = pd.to_numeric(df['tstamp'])
        timestamps = timestamps - timestamps.min()

        print("Normalizing metrics...")
        # Normalize each metric
        norm_rsrp = normalize_data(df['rsrp'].to_numpy())
        norm_cqi = normalize_data(df['cqi'].to_numpy())
        norm_ul_tbs = normalize_data(df['ul_curr_tbs'].to_numpy())
        norm_pusch_snr = normalize_data(df['pusch_snr'].to_numpy())

        # Calculate statistics
        print("Calculating statistics...")
        original_stats = {
            'rsrp': {
                'min': float(df['rsrp'].min()),
                'max': float(df['rsrp'].max()),
                'mean': float(df['rsrp'].mean()),
                'removed_zeros': int((df['rsrp'] == 0).sum()),
                'total_samples': len(df['rsrp'])
            },
            'cqi': {
                'min': float(df['cqi'].min()),
                'max': float(df['cqi'].max()),
                'mean': float(df['cqi'].mean()),
                'removed_zeros': int((df['cqi'] == 0).sum()),
                'total_samples': len(df['cqi'])
            },
            'ul_curr_tbs': {
                'min': float(df['ul_curr_tbs'].min()),
                'max': float(df['ul_curr_tbs'].max()),
                'mean': float(df['ul_curr_tbs'].mean()),
                'removed_zeros': int((df['ul_curr_tbs'] == 0).sum()),
                'total_samples': len(df['ul_curr_tbs'])
            },
            'pusch_snr': {
                'min': float(df['pusch_snr'].min()),
                'max': float(df['pusch_snr'].max()),
                'mean': float(df['pusch_snr'].mean()),
                'removed_zeros': int((df['pusch_snr'] == 0).sum()),
                'total_samples': len(df['pusch_snr'])
            }
        }

        return timestamps.to_numpy(), norm_rsrp, norm_cqi, norm_ul_tbs, norm_pusch_snr, original_stats

    except sqlite3.OperationalError as e:
        print(f"\nDatabase operational error: {e}")
        print("Try running the script with sudo if it's a permission issue")
        return None, None, None, None, None, None
    except Exception as e:
        print(f"\nError fetching data: {e}")
        return None, None, None, None, None, None

def save_plot_metadata(metadata_path: str, db_path: str, timestamp: str, stats: dict):
    """Save metadata about the plot"""
    metadata = {
        "timestamp": timestamp,
        "database": os.path.basename(db_path),
        "metrics": ["rsrp", "cqi", "ul_curr_tbs", "pusch_snr"],
        "table": "MAC_UE",
        "normalization": "min-max scaling to [0,1]",
        "original_statistics": stats,
        "processing": "Filtered out zero values for all metrics"
    }
    
    with open(metadata_path, "w") as f:
        json.dump(metadata, f, indent=4)

def plot_normalized_metrics(db_path: str):
    """Plot normalized RSRP, CQI, UL_CURR_TBS, and PUSCH_SNR"""
    print("\nFetching and processing data...")
    times, norm_rsrp, norm_cqi, norm_ul_tbs, norm_pusch_snr, stats = fetch_mac_ue_data(db_path)
    
    if times is None:
        print("Failed to fetch data")
        return

    print("\nCreating plot...")
    # Create plot
    fig, ax = plt.subplots(figsize=(12, 8))

    # Plot each metric
    ax.plot(times, norm_rsrp, label='Normalized RSRP', alpha=0.7)
    ax.plot(times, norm_cqi, label='Normalized CQI', alpha=0.7)
    ax.plot(times, norm_ul_tbs, label='Normalized UL_CURR_TBS', alpha=0.7)
    ax.plot(times, norm_pusch_snr, label='Normalized PUSCH SNR', alpha=0.7)

    # Configure plot
    ax.set_xlabel('Time (s)')
    ax.set_ylabel('Normalized Values')
    ax.set_title('Comparison of Normalized Metrics from MAC_UE\n(Zero values filtered out)')
    ax.grid(True, alpha=0.3)
    ax.legend(loc='best', framealpha=0.9)

    # Create norm_metrics directory if it doesn't exist
    save_dir = "norm_metrics"
    os.makedirs(save_dir, exist_ok=True)
    
    # Generate timestamp for unique filename
    timestamp = datetime.now().strftime("%Y%m%d-%H%M%S")
    
    print("\nSaving results...")
    # Save plot
    plot_filename = f"normalized_mac_ue_metrics_{timestamp}.png"
    filepath = os.path.join(save_dir, plot_filename)
    fig.savefig(filepath, bbox_inches='tight', dpi=300)
    
    # Save metadata
    metadata_filename = f"metadata_{timestamp}.json"
    metadata_path = os.path.join(save_dir, metadata_filename)
    save_plot_metadata(metadata_path, db_path, timestamp, stats)
    
    print(f"\nPlot saved as: {filepath}")
    print(f"Metadata saved as: {metadata_path}")
    
    # Print summary statistics
    print("\nSummary of filtered data:")
    for metric, metric_stats in stats.items():
        print(f"\n{metric.upper()}:")
        print(f"  Min: {metric_stats['min']:.2f}")
        print(f"  Max: {metric_stats['max']:.2f}")
        print(f"  Mean: {metric_stats['mean']:.2f}")
        print(f"  Total samples: {metric_stats['total_samples']:,}")
        print(f"  Zeros removed: {metric_stats['removed_zeros']:,}")
    
    plt.show()

# Example usage
if __name__ == "__main__":
    while True:
        db_path = input("Please enter the database path: ")
        if os.path.exists(db_path):
            break
        print(f"Error: File not found at {db_path}")
        print("Please enter a valid path")
    
    print("\nAttempting to read database...")
    plot_normalized_metrics(db_path)
