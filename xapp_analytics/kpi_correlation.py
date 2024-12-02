import sqlite3
import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
from scipy import stats
import numpy as np
from typing import List
import os
import datetime

def show_table_fields(db_path: str, table_name: str) -> List[str]:
    """Show and return the field names of a table"""
    try:
        conn = sqlite3.connect(db_path)
        cursor = conn.cursor()
        cursor.execute(f"PRAGMA table_info({table_name})")
        columns_info = cursor.fetchall()
        
        print(f"\nFields in table '{table_name}':")
        print("-" * 50)
        print("Index | Name        | Type")
        print("-" * 50)
        
        field_names = []
        for col in columns_info:
            index, name, dtype, *_ = col
            field_names.append(name)
            print(f"{index:5d} | {name:<11} | {dtype}")
        
        conn.close()
        return field_names
    except sqlite3.Error as e:
        print(f"Error accessing table {table_name}: {e}")
        return []

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
def analyze_correlation(db_path: str):
    """Analyze correlation between two fields in a SQLite database"""
    try:
        # Get available tables
        tables = get_tables_from_db(db_path)
        if not tables:
            print("No tables found in the database.")
            return

        # Show available tables
        print("\nAvailable tables:")
        for idx, table in enumerate(tables, 1):
            print(f"{idx}. {table}")

        # Select table
        while True:
            try:
                table_idx = int(input("\nSelect table number: ")) - 1
                if 0 <= table_idx < len(tables):
                    selected_table = tables[table_idx]
                    break
                print("Invalid table number.")
            except ValueError:
                print("Please enter a valid number.")

        # Get and show fields
        fields = show_table_fields(db_path, selected_table)
        if not fields:
            return

        # Select fields for correlation
        print("\nSelect two fields for correlation analysis:")
        while True:
            try:
                field1_idx = int(input(f"Select first field (0-{len(fields)-1}): "))  # Subtract 1 here
                field2_idx = int(input(f"Select second field (0-{len(fields)-1}): "))  # Subtract 1 here
                
                if 0 <= field1_idx < len(fields) and 0 <= field2_idx < len(fields):
                    field1 = fields[field1_idx]
                    field2 = fields[field2_idx]
                    break
                print("Invalid field numbers.")
            except ValueError:
                print("Please enter valid numbers.")

        # Fetch data
        conn = sqlite3.connect(db_path)
        query = f"SELECT {field1}, {field2} FROM {selected_table} WHERE {field1} IS NOT NULL AND {field2} IS NOT NULL"
        df = pd.read_sql_query(query, conn)
        conn.close()

        # Convert to numeric, dropping non-numeric values
        df = df.apply(pd.to_numeric, errors='coerce')
        df = df.dropna()

        if df.empty:
            print("No valid numeric data found for correlation analysis.")
            return

        # Calculate correlation
        correlation = df[field1].corr(df[field2])
        
        # Calculate additional statistics
        slope, intercept, r_value, p_value, std_err = stats.linregress(df[field1], df[field2])
        r_squared = r_value ** 2

        # Create visualization
        plt.figure(figsize=(12, 8))
        
        # Scatter plot
        plt.scatter(df[field1], df[field2], alpha=0.5)
        
        # Regression line
        x_line = np.linspace(df[field1].min(), df[field1].max(), 100)
        y_line = slope * x_line + intercept
        plt.plot(x_line, y_line, color='red', label=f'Regression line (R² = {r_squared:.3f})')

        plt.title(f'Correlation between {field1} and {field2}')
        plt.xlabel(field1)
        plt.ylabel(field2)
        plt.legend()

        # Add correlation information to plot
        info_text = (f'Correlation: {correlation:.3f}\n'
                    f'R-squared: {r_squared:.3f}\n'
                    f'P-value: {p_value:.3e}')
        plt.text(0.05, 0.95, info_text, transform=plt.gca().transAxes, 
                bbox=dict(facecolor='white', alpha=0.8),
                verticalalignment='top')

        # Save results
        timestamp = datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
        results_dir = "correlation_results"
        os.makedirs(results_dir, exist_ok=True)
        
        # Save plot - CORRECTED PART
        plt.tight_layout()
        save_path = f"{results_dir}/correlation_plot_{timestamp}.png"
        plt.savefig(save_path, bbox_inches='tight', dpi=300)
        
        # Show plot
        plt.show()
        
        # Close the plot to free memory
        plt.close()

        # Print correlation analysis
        print("\nCorrelation Analysis Results:")
        print("-" * 50)
        print(f"Correlation coefficient: {correlation:.3f}")
        print(f"R-squared value: {r_squared:.3f}")
        print(f"P-value: {p_value:.3e}")
        print(f"Standard error: {std_err:.3f}")
        
        # Interpret correlation
        print("\nInterpretation:")
        if abs(correlation) < 0.3:
            strength = "weak"
        elif abs(correlation) < 0.7:
            strength = "moderate"
        else:
            strength = "strong"
            
        direction = "positive" if correlation > 0 else "negative"
        
        print(f"There is a {strength} {direction} correlation between {field1} and {field2}.")
        
        if p_value < 0.05:
            print("The correlation is statistically significant (p < 0.05).")
        else:
            print("The correlation is not statistically significant (p >= 0.05).")
        
        # Save data
        results = {
            'field1': field1,
            'field2': field2,
            'correlation': correlation,
            'r_squared': r_squared,
            'p_value': p_value,
            'std_err': std_err
        }
        
        with open(f"{results_dir}/correlation_results_{timestamp}.txt", 'w') as f:
            for key, value in results.items():
                f.write(f"{key}: {value}\n")

        print(f"\nResults saved in {results_dir} directory")

    except Exception as e:
        print(f"Error during analysis: {e}")

if __name__ == "__main__":
    db_path = input("Enter the path to your SQLite database: ")
    if os.path.exists(db_path):
        analyze_correlation(db_path)
    else:
        print("Database file not found.")
