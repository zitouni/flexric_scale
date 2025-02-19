import sqlite3
import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
import numpy as np
from typing import List, Dict, Tuple
import os
import datetime
import logging
from tqdm import tqdm

# Suppress TensorFlow GPU warnings
os.environ['TF_CPP_MIN_LOG_LEVEL'] = '2'

class AdvancedCorrelationAnalyzer:
    def __init__(self, db_path: str):
        self.db_path = db_path
        self.setup_logging()
        
    def setup_logging(self):
        """Setup logging configuration"""
        logging.basicConfig(
            level=logging.INFO,
            format='%(asctime)s - %(levelname)s - %(message)s',
            handlers=[
                logging.FileHandler('correlation_analysis.log'),
                logging.StreamHandler()
            ]
        )
    def interpret_correlation(self,corr: float) -> str:
        """Interpret the type and strength of correlation"""
        abs_corr = abs(corr)
        direction = "positive" if corr > 0 else "negative"

        if abs_corr > 0.9:
            strength = "Very Strong"
        elif abs_corr > 0.7:
            strength = "Strong"
        elif abs_corr > 0.5:
            strength = "Moderate"
        elif abs_corr > 0.3:
            strength = "Weak"
        else:
            strength = "Very Weak"
            
        return f"{strength} {direction}"
    
    def get_numeric_columns(self, table_name: str) -> List[str]:
        """Get all numeric columns from the table"""
        try:
            print("\nFetching numeric columns...")
            query = f"SELECT * FROM {table_name} LIMIT 1"
            conn = sqlite3.connect(self.db_path)
            df = pd.read_sql_query(query, conn)
            conn.close()
            
            numeric_cols = df.select_dtypes(include=[np.number]).columns.tolist()
            return numeric_cols
        except Exception as e:
            logging.error(f"Error getting numeric columns: {e}")
            return []

    def load_data(self, table_name: str, columns: List[str]) -> pd.DataFrame:
        """Load data from specified columns with progress bar"""
        try:
            print("\nLoading data...")
            conn = sqlite3.connect(self.db_path)
            df = pd.read_sql_query(f"SELECT {', '.join(columns)} FROM {table_name}", conn)
            conn.close()
            
            # Remove rows with NaN values
            df = df.dropna()
            return df
            
        except Exception as e:
            logging.error(f"Error loading data: {e}")
            return pd.DataFrame()

    def calculate_correlation_matrix(self, df: pd.DataFrame) -> np.ndarray:
        """Calculate correlation matrix with progress indicator"""
        print("\nCalculating correlation matrix...")
        with tqdm(total=1, desc="Computing correlations") as pbar:
            correlation_matrix = df.corr()
            pbar.update(1)
        return correlation_matrix

    def find_highly_correlated_pairs(self, 
                                   correlation_matrix: pd.DataFrame, 
                                   threshold: float = 0.7) -> List[Tuple[str, str, float]]:
        """Find pairs of highly correlated features"""
        correlations = []
        
        print("\nFinding highly correlated pairs...")
        for i in range(len(correlation_matrix.columns)):
            for j in range(i + 1, len(correlation_matrix.columns)):
                correlation = abs(correlation_matrix.iloc[i, j])
                if correlation > threshold:
                    correlations.append((
                        correlation_matrix.columns[i],
                        correlation_matrix.columns[j],
                        correlation
                    ))
        
        return sorted(correlations, key=lambda x: abs(x[2]), reverse=True)
    
    def plot_key_correlations(self, 
                            df: pd.DataFrame, 
                            highly_correlated: List[Tuple[str, str, float]], 
                            timestamp: str):
        """Plot scatter plots for top correlations efficiently"""
        if not highly_correlated:
            return

        print("\nGenerating scatter plots for top correlations...")
        results_dir = "correlation_results"
        os.makedirs(results_dir, exist_ok=True)

        # Take only top 3 most correlated pairs
        top_pairs = highly_correlated[:3]

        # Create a single figure with subplots
        fig, axes = plt.subplots(1, len(top_pairs), figsize=(15, 5))
        if len(top_pairs) == 1:
            axes = [axes]

        with tqdm(total=len(top_pairs), desc="Creating scatter plots") as pbar:
            for (param1, param2, corr), ax in zip(top_pairs, axes):
                # Sample data if there are too many points
                if len(df) > 1000:
                    sample_idx = np.random.choice(len(df), 1000, replace=False)
                    x_data = df[param1].iloc[sample_idx]
                    y_data = df[param2].iloc[sample_idx]
                else:
                    x_data = df[param1]
                    y_data = df[param2]

                # Create scatter plot
                ax.scatter(x_data, y_data, alpha=0.5, s=20)
                
                # Add trend line
                z = np.polyfit(x_data, y_data, 1)
                x_range = np.array([x_data.min(), x_data.max()])
                ax.plot(x_range, z[0] * x_range + z[1], "r--", alpha=0.8)
                
                # Set labels
                ax.set_xlabel(param1, fontsize=8)
                ax.set_ylabel(param2, fontsize=8)
                
                # Add correlation interpretation to title
                corr_type = self.interpret_correlation(corr)
                ax.set_title(f'Correlation: {corr:.3f}\n({corr_type})', fontsize=10)
                
                # Rotate labels if needed
                if max(len(str(label)) for label in ax.get_xticklabels()) > 10:
                    ax.tick_params(axis='x', rotation=45)
                
                pbar.update(1)

        plt.tight_layout()
        plt.savefig(f"{results_dir}/top_correlations_{timestamp}.png",
                    bbox_inches='tight',
                    dpi=200)
        plt.close()
   
    def plot_correlation_heatmap(self, 
                               correlation_matrix: pd.DataFrame, 
                               timestamp: str):
        """Plot correlation heatmap"""
        print("\nGenerating correlation heatmap...")
        plt.figure(figsize=(12, 10))
        
        # Create heatmap
        sns.heatmap(correlation_matrix,
                   cmap='coolwarm',
                   center=0,
                   annot=True,
                   fmt='.2f')
        
        plt.title('Correlation Heatmap of MAC_UE Parameters')
        plt.xticks(rotation=45, ha='right')
        plt.yticks(rotation=0)
        
        # Save the plot
        results_dir = "correlation_results"
        os.makedirs(results_dir, exist_ok=True)
        plt.savefig(f"{results_dir}/correlation_heatmap_{timestamp}.png",
                    bbox_inches='tight',
                    dpi=200)
        plt.close()

    def analyze_correlations(self):
        """Main method to analyze correlations"""
        try:
            print("\nStarting correlation analysis...")
            timestamp = datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
            
            # Get numeric columns
            numeric_columns = self.get_numeric_columns("MAC_UE")
            if not numeric_columns:
                logging.error("No numeric columns found in MAC_UE table")
                return
            
            # Load data
            df = self.load_data("MAC_UE", numeric_columns)
            if df.empty:
                logging.error("No data loaded from MAC_UE table")
                return
            
            # Calculate correlation matrix
            correlation_matrix = self.calculate_correlation_matrix(df)
            
            # Find highly correlated pairs
            highly_correlated = self.find_highly_correlated_pairs(correlation_matrix)
            
            # Generate plots
            self.plot_correlation_heatmap(correlation_matrix, timestamp)
            self.plot_key_correlations(df, highly_correlated, timestamp)
            
            # Save results with correlation interpretation
            results_dir = "correlation_results"
            os.makedirs(results_dir, exist_ok=True)
            
            with open(f"{results_dir}/correlation_analysis_{timestamp}.txt", 'w') as f:
                f.write("Highly Correlated Parameters in MAC_UE Table\n")
                f.write("=" * 50 + "\n\n")
                
                if highly_correlated:
                    for param1, param2, corr in highly_correlated:
                        corr_type = self.interpret_correlation(corr)
                        f.write(f"{param1} ←→ {param2}: {corr:.3f} ({corr_type})\n")
                else:
                    f.write("No highly correlated parameters found\n")
            
            # Print results with correlation interpretation
            print("\nHighly Correlated Parameters:")
            print("=" * 50)
            if highly_correlated:
                for param1, param2, corr in highly_correlated:
                    corr_type = self.interpret_correlation(corr)
                    print(f"{param1} ←→ {param2}: {corr:.3f} ({corr_type})")
            else:
                print("No highly correlated parameters found")
                
            print(f"\nResults saved in {results_dir} directory")
            
        except Exception as e:
            logging.error(f"Error during analysis: {e}")


def main():
    db_path = input("Enter the path to your SQLite database: ")
    if not os.path.exists(db_path):
        print("Database file not found.")
        return
        
    analyzer = AdvancedCorrelationAnalyzer(db_path)
    analyzer.analyze_correlations()

if __name__ == "__main__":
    main()
