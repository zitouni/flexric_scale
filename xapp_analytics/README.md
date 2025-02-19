# Network KPI Analysis and Visualization Toolkit

This project provides a comprehensive set of tools for analyzing and visualizing network Key Performance Indicators (KPIs) from SQLite databases. It offers correlation analysis, data normalization, clustering, and trend prediction capabilities for network performance metrics.

## Repository Structure

The repository contains several Python scripts, each focusing on different aspects of network KPI analysis:

- `global_kpi_correlation.py`: Performs advanced correlation analysis on KPI data.
- `throughput_plot.py`: Generates throughput plots from database data.
- `tensorFlowKpi.py`: Analyzes KPI data using TensorFlow, including clustering and trend prediction.
- `kpi_correlation.py`: Analyzes correlation between two selected fields in the database.
- `compare_metrics.py`: Compares and visualizes normalized metrics from the MAC_UE table.
- `shrink_database.py`: Reduces the size of the SQLite database by selecting records at specified intervals.
- `compareKpi.py`: Fetches and analyzes various network performance metrics.

## Usage Instructions

### Installation

1. Ensure you have Python 3.7+ installed.
2. Install required dependencies:
   ```
   pip install pandas numpy matplotlib seaborn scipy tensorflow
   ```

### Getting Started

1. Clone the repository:
   ```
   git clone <repository_url>
   cd <repository_directory>
   ```

2. Run the desired script:
   ```
   python <script_name>.py
   ```

3. Follow the prompts to input the path to your SQLite database and select the desired analysis options.

### Common Use Cases

1. Correlation Analysis:
   ```
   python global_kpi_correlation.py
   ```
   This will perform a comprehensive correlation analysis on all numeric fields in the specified table.

2. Throughput Visualization:
   ```
   python throughput_plot.py
   ```
   Generate throughput plots based on data from the SQLite database.

3. KPI Clustering and Trend Analysis:
   ```
   python tensorFlowKpi.py
   ```
   Perform clustering analysis on KPI data and predict trends using LSTM models.

4. Comparing Normalized Metrics:
   ```
   python compare_metrics.py
   ```
   Visualize normalized RSRP, CQI, UL_CURR_TBS, and PUSCH_SNR metrics from the MAC_UE table.

### Troubleshooting

1. Database Connection Issues:
   - Ensure the database path is correct and the file exists.
   - Check if you have read permissions for the database file.
   - If using `shrink_database.py`, ensure you have write permissions for the target directory.

2. Memory Errors:
   - For large databases, increase the `chunk_size` in scripts that process data in chunks.
   - Consider using a machine with more RAM for very large datasets.

3. TensorFlow Warnings:
   - If you encounter TensorFlow GPU warnings, set the environment variable:
     ```
     export TF_CPP_MIN_LOG_LEVEL='2'
     ```

### Performance Optimization

- Use `shrink_database.py` to reduce the size of large databases before analysis.
- Adjust the `chunk_size` in scripts that support chunked processing to balance between memory usage and processing speed.
- For `tensorFlowKpi.py`, consider using a GPU-enabled setup for faster processing of large datasets.

## Data Flow

The typical data flow in this toolkit follows these steps:

1. User inputs the path to the SQLite database.
2. The selected script connects to the database and fetches relevant data.
3. Data is preprocessed (e.g., normalization, filtering out zero values).
4. Analysis is performed (correlation, clustering, trend prediction).
5. Results are visualized using matplotlib or seaborn.
6. Plots and analysis results are saved to the local filesystem.

```
[User Input] -> [Database Connection] -> [Data Fetching] -> [Preprocessing]
    -> [Analysis] -> [Visualization] -> [Save Results]
```

Each script may focus on different parts of this flow, but the general pattern remains consistent across the toolkit.