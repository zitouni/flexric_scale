import sqlite3
import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import seaborn as sns
from typing import List, Dict
import logging
import datetime

import tensorflow as tf
from tensorflow.keras.layers import Input, Dense, LSTM
from tensorflow.keras.models import Model

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
        normalizer = tf.keras.layers.Normalization(axis=-1)
        normalizer.adapt(df[numerical_cols].values)
        df[numerical_cols] = normalizer(df[numerical_cols].values).numpy()

        return df

    def perform_clustering(self, df: pd.DataFrame, n_clusters: int = 3) -> Dict:
        numerical_cols = df.select_dtypes(include=['float64', 'int64']).columns
        X = df[numerical_cols].values
        
        # Build autoencoder for dimensionality reduction
        input_dim = X.shape[1]
        encoding_dim = 2
        
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
            'explained_variance': None  # Not directly available in TF implementation
        }

    def visualize_clusters(self, df: pd.DataFrame, cluster_results: Dict):
        current_time = datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
        log_dir = f'logs/scalars/{current_time}'
        summary_writer = tf.summary.create_file_writer(log_dir)
        
        with summary_writer.as_default():
            for i in range(cluster_results['encoded_data'].shape[1]):
                tf.summary.histogram(f'encoded_dim_{i}', cluster_results['encoded_data'][:, i], step=0)
            
            tf.summary.scalar('num_clusters', len(np.unique(cluster_results['clusters'])), step=0)

        print(f"TensorBoard logs saved to {log_dir}. Run 'tensorboard --logdir {log_dir}' to view.")

        # Keep the matplotlib visualization for quick view
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

    def analyze_kpi_trends(self, df: pd.DataFrame, kpi_columns: List[str]):
        for kpi in kpi_columns:
            if kpi in df.columns:
                kpi_data = df[kpi].values.reshape(-1, 1)
                
                # Create and train a simple LSTM model
                model = tf.keras.Sequential([
                    LSTM(50, activation='relu', input_shape=(1, 1)),
                    Dense(1)
                ])
                model.compile(optimizer='adam', loss='mse')
                
                # Prepare data for LSTM (assuming time series)
                X = kpi_data[:-1]
                y = kpi_data[1:]
                X = X.reshape((X.shape[0], 1, 1))
                
                model.fit(X, y, epochs=100, verbose=0)
                
                # Make predictions
                test_input = kpi_data[-1].reshape((1, 1, 1))
                predictions = []
                for _ in range(10):  # Predict next 10 points
                    prediction = model.predict(test_input)
                    predictions.append(prediction[0, 0])
                    test_input = prediction.reshape((1, 1, 1))
                
                # Plot results
                plt.figure(figsize=(12, 6))
                plt.plot(range(len(kpi_data)), kpi_data, label='Actual')
                plt.plot(range(len(kpi_data)-1, len(kpi_data)+len(predictions)-1), predictions, label='Predicted')
                plt.title(f'{kpi} Trend Analysis')
                plt.xlabel('Time')
                plt.ylabel(kpi)
                plt.legend()
                plt.show()

def main():
    # Example usage
    db_paths = [
        '/path/to/database1.db',
        '/path/to/database2.db',
        '/path/to/database3.db'
    ]
    
    # Initialize analyzer
    analyzer = NetworkKPIAnalyzer(db_paths)
    
    # Fetch and combine data from all databases
    table_name = 'kpi_measurements'  # Replace with your actual table name
    combined_data = analyzer.fetch_kpi_data(table_name)
    
    if combined_data is not None:
        # Preprocess the data
        processed_data = analyzer.preprocess_data(combined_data)
        
        # Perform clustering analysis
        cluster_results = analyzer.perform_clustering(processed_data)
        
        # Visualize the results
        analyzer.visualize_clusters(processed_data, cluster_results)
        
        # Analyze specific KPI trends
        kpi_columns = ['throughput', 'latency', 'rsrp', 'sinr']  # Replace with your actual KPI columns
        analyzer.analyze_kpi_trends(processed_data, kpi_columns)
        
        # Print summary statistics
        print("\nSummary Statistics:")
        print(processed_data.describe())
        
        # Calculate correlations between KPIs
        correlation_matrix = processed_data[kpi_columns].corr()
        plt.figure(figsize=(10, 8))
        sns.heatmap(correlation_matrix, annot=True, cmap='coolwarm')
        plt.title('KPI Correlation Matrix')
        plt.tight_layout()
        plt.show()

if __name__ == "__main__":
    main()
