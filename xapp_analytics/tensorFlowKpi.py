import sqlite3
import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import seaborn as sns
from typing import List, Dict, Tuple
import logging
import datetime
import os
import tensorflow as tf
from tensorflow.keras.layers import Input, Dense, LSTM
from tensorflow.keras.models import Model

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
        print(f"Error accessing database {db_path}: {e}")
        return []

def get_db_and_table_selections() -> List[Tuple[str, str]]:
    """Get database paths and corresponding table selections from user"""
    db_selections = []
    
    # Define default paths
    default_paths = [
        '/home/rz0005/OneDrive/ICS/TUDOR/Measurements/xapp_mac_terre_split_mob_2',
        '/home/rz0005/OneDrive/ICS/TUDOR/Measurements/xapp_mac_terre_split_static'
    ]

    print("\nAvailable default database paths:")
    for i, path in enumerate(default_paths, 1):
        print(f"{i}. {path}")
    print("3. Enter custom path")

    while True:
        try:
            num_dbs = int(input("\nEnter the number of databases to analyze: "))
            if num_dbs <= 0:
                print("Please enter a positive number.")
                continue
            break
        except ValueError:
            print("Please enter a valid number.")

    for i in range(num_dbs):
        while True:
            try:
                print(f"\nFor database {i+1}:")
                choice = int(input("Select option (1-3): "))
                if choice in [1, 2]:
                    db_path = default_paths[choice-1]
                elif choice == 3:
                    db_path = input("Enter custom database path: ")
                else:
                    print("Invalid choice. Please select 1-3.")
                    continue

                if not os.path.exists(db_path):
                    print(f"Error: File {db_path} does not exist.")
                    continue

                # Get and display available tables
                tables = get_tables_from_db(db_path)
                if not tables:
                    print(f"No tables found in {db_path}")
                    continue

                print("\nAvailable tables:")
                for idx, table in enumerate(tables, 1):
                    print(f"{idx}. {table}")

                # Get table selection
                while True:
                    try:
                        table_idx = int(input("Select table number: ")) - 1
                        if 0 <= table_idx < len(tables):
                            selected_table = tables[table_idx]
                            break
                        print("Invalid table number.")
                    except ValueError:
                        print("Please enter a valid number.")

                db_selections.append((db_path, selected_table))
                break

            except Exception as e:
                print(f"Error: {str(e)}")
                print("Please try again.")

    return db_selections

class NetworkKPIAnalyzer:
    def __init__(self, db_paths: List[str]):
        self.db_paths = db_paths
        self.combined_data = None
        self.setup_logging()

    def setup_logging(self):
        logging.basicConfig(
            level=logging.INFO,
            format='%(asctime)s - %(levelname)s - %(message)s'
        )

    def connect_to_db(self, db_path: str) -> sqlite3.Connection:
        try:
            return sqlite3.connect(db_path)
        except sqlite3.Error as e:
            logging.error(f"Error connecting to database {db_path}: {e}")
            raise

    def fetch_kpi_data(self, table_name: str) -> pd.DataFrame:
        all_data = []
        
        for db_path in self.db_paths:
            try:
                conn = self.connect_to_db(db_path)
                query = f"SELECT * FROM {table_name}"
                df = pd.read_sql_query(query, conn)
                df['source_db'] = db_path  # Add source database identifier
                all_data.append(df)
                conn.close()
            except Exception as e:
                logging.error(f"Error reading data from {db_path}: {e}")
                continue
                
        return pd.concat(all_data, ignore_index=True) if all_data else None

    def preprocess_data(self, df: pd.DataFrame) -> pd.DataFrame:
        if df is None or df.empty:
            raise ValueError("No data available for preprocessing")

        # Remove duplicates
        df = df.drop_duplicates()

        # Handle missing values
        df = df.fillna(df.mean(numeric_only=True))

        # Convert categorical variables to numeric
        categorical_columns = df.select_dtypes(include=['object']).columns
        for col in categorical_columns:
            if col != 'source_db':  # Preserve source database information
                df[col] = pd.factorize(df[col])[0]

        # Normalize numerical columns
        numerical_cols = df.select_dtypes(include=['float64', 'int64']).columns
        if len(numerical_cols) > 0:  # Only normalize if numerical columns exist
            normalizer = tf.keras.layers.Normalization(axis=-1)
            normalizer.adapt(df[numerical_cols].values)
            df[numerical_cols] = normalizer(df[numerical_cols].values).numpy()

        return df

    def perform_clustering(self, df: pd.DataFrame, n_clusters: int = 3) -> Dict:
        numerical_cols = df.select_dtypes(include=['float64', 'int64']).columns
        if len(numerical_cols) == 0:
            raise ValueError("No numerical columns available for clustering")
            
        X = df[numerical_cols].values
        
        # Build autoencoder for dimensionality reduction
        input_dim = X.shape[1]
        encoding_dim = min(2, input_dim)  # Ensure encoding_dim doesn't exceed input_dim
        
        input_layer = Input(shape=(input_dim,))
        encoded = Dense(encoding_dim, activation='relu')(input_layer)
        decoded = Dense(input_dim, activation='linear')(encoded)
        
        autoencoder = Model(input_layer, decoded)
        encoder = Model(input_layer, encoded)
        
        autoencoder.compile(optimizer='adam', loss='mse')
        autoencoder.fit(X, X, epochs=50, batch_size=32, shuffle=True, verbose=0)
        
        # Get encoded representation
        encoded_data = encoder.predict(X)
        
        # Perform K-means clustering on encoded data
        kmeans = tf.keras.layers.experimental.preprocessing.KMeans(n_clusters=n_clusters)
        clusters = kmeans(encoded_data)

        return {
            'clusters': clusters.numpy(),
            'encoded_data': encoded_data,
            'explained_variance': None
        }

    def visualize_clusters(self, df: pd.DataFrame, cluster_results: Dict):
        if cluster_results['encoded_data'].shape[1] >= 2:
            plt.figure(figsize=(10, 8))
            scatter = plt.scatter(
                cluster_results['encoded_data'][:, 0],
                cluster_results['encoded_data'][:, 1],
                c=cluster_results['clusters'],
                cmap='viridis'
            )
            plt.title('KPI Clusters Visualization (Autoencoder)')
            plt.xlabel('Encoded Dimension 1')
            plt.ylabel('Encoded Dimension 2')
            plt.colorbar(scatter)
            plt.tight_layout()
            plt.show()

        # Save TensorBoard logs
        current_time = datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
        log_dir = f'logs/scalars/{current_time}'
        summary_writer = tf.summary.create_file_writer(log_dir)
        
        with summary_writer.as_default():
            for i in range(cluster_results['encoded_data'].shape[1]):
                tf.summary.histogram(f'encoded_dim_{i}', 
                                   cluster_results['encoded_data'][:, i], 
                                   step=0)
            tf.summary.scalar('num_clusters', 
                            len(np.unique(cluster_results['clusters'])), 
                            step=0)

    def analyze_kpi_trends(self, df: pd.DataFrame, kpi_columns: List[str]):
        for kpi in kpi_columns:
            if kpi in df.columns:
                try:
                    kpi_data = df[kpi].values.reshape(-1, 1)
                    
                    if len(kpi_data) < 2:  # Skip if not enough data points
                        print(f"Insufficient data points for {kpi}")
                        continue

                    # Create and train a simple LSTM model
                    model = tf.keras.Sequential([
                        LSTM(50, activation='relu', input_shape=(1, 1)),
                        Dense(1)
                    ])
                    model.compile(optimizer='adam', loss='mse')
                    
                    # Prepare data for LSTM
                    X = kpi_data[:-1]
                    y = kpi_data[1:]
                    X = X.reshape((X.shape[0], 1, 1))
                    
                    model.fit(X, y, epochs=100, verbose=0)
                    
                    # Make predictions
                    test_input = kpi_data[-1].reshape((1, 1, 1))
                    predictions = []
                    for _ in range(10):  # Predict next 10 points
                        prediction = model.predict(test_input, verbose=0)
                        predictions.append(prediction[0, 0])
                        test_input = prediction.reshape((1, 1, 1))
                    
                    # Plot results
                    plt.figure(figsize=(12, 6))
                    plt.plot(range(len(kpi_data)), kpi_data, label='Actual')
                    plt.plot(range(len(kpi_data)-1, len(kpi_data)+len(predictions)-1), 
                            predictions, label='Predicted')
                    plt.title(f'{kpi} Trend Analysis')
                    plt.xlabel('Time')
                    plt.ylabel(kpi)
                    plt.legend()
                    plt.show()
                except Exception as e:
                    print(f"Error analyzing {kpi}: {str(e)}")

def main():
    # Get database and table selections
    db_selections = get_db_and_table_selections()
    
    if not db_selections:
        print("No valid database selections made.")
        return

    # Initialize analyzer with selected database paths
    db_paths = [selection[0] for selection in db_selections]
    analyzer = NetworkKPIAnalyzer(db_paths)

    # Process each database with its selected table
    all_processed_data = []
    for db_path, table_name in db_selections:
        print(f"\nAnalyzing {table_name} from {db_path}")
        
        try:
            df = analyzer.fetch_kpi_data(table_name)
            if df is not None and not df.empty:
                print(f"Found {len(df)} records")
                processed_df = analyzer.preprocess_data(df)
                all_processed_data.append(processed_df)
            else:
                print(f"No data found in table {table_name}")
        except Exception as e:
            print(f"Error processing {db_path}: {str(e)}")
            continue

    if all_processed_data:
        try:
            combined_df = pd.concat(all_processed_data, ignore_index=True)
            
            print("\nPerforming clustering analysis...")
            n_clusters = int(input("Enter the number of clusters (default is 3): ") or 3)
            cluster_results = analyzer.perform_clustering(combined_df, n_clusters)
            
            print("\nGenerating visualizations...")
            analyzer.visualize_clusters(combined_df, cluster_results)
            
            numerical_cols = combined_df.select_dtypes(include=['float64', 'int64']).columns
            if len(numerical_cols) > 0:
                print("\nAvailable numerical columns for trend analysis:")
                for i, col in enumerate(numerical_cols, 1):
                    print(f"{i}. {col}")
                
                selected_cols = []
                while True:
                    try:
                        selections = input("\nEnter column numbers to analyze (comma-separated) or press Enter to analyze all: ").strip()
                        if not selections:
                            selected_cols = list(numerical_cols)
                            break
                        indices = [int(x.strip())-1 for x in selections.split(',')]
                        selected_cols = [numerical_cols[i] for i in indices if 0 <= i < len(numerical_cols)]
                        if selected_cols:
                            break
                        print("No valid columns selected.")
                    except ValueError:
                        print("Please enter valid numbers.")
                
                print("\nAnalyzing KPI trends...")
                analyzer.analyze_kpi_trends(combined_df, selected_cols)
                
                # Save results
                timestamp = datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
                results_dir = "analysis_results"
                os.makedirs(results_dir, exist_ok=True)
                
                output_file = os.path.join(results_dir, f"combined_analysis_{timestamp}.csv")
                combined_df.to_csv(output_file, index=False)
                print(f"\nResults saved to: {output_file}")
                
                # Display correlation matrix
                correlation_matrix = combined_df[selected_cols].corr()
                plt.figure(figsize=(12, 10))
                sns.heatmap(correlation_matrix, annot=True, cmap='coolwarm')
                plt.title('KPI Correlation Matrix')
                plt.tight_layout()
                plt.show()
            else:
                print("No numerical columns available for analysis")
                
        except Exception as e:
            print(f"Error during analysis: {str(e)}")
    else:
        print("No data available for analysis.")

if __name__ == "__main__":
    main()
