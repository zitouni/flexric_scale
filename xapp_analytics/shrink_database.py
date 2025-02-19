import sqlite3
import os
from datetime import datetime

def check_path_exists(path: str) -> bool:
    """
    Check if the directory path exists.
    
    Args:
        path: Directory path to check
    Returns:
        bool: True if path exists, False otherwise
    """
    directory = os.path.dirname(path)
    return os.path.exists(directory)

def get_valid_paths():
    """
    Get and validate source database and target paths from user input.
    Proposes a filename with timestamp (date_time) for the target database.
    
    Returns:
        tuple: (source_db_path, target_db_path)
    """
    while True:
        # Get and validate source path
        source_db = input("\nEnter the path to the source database: ").strip()
        source_db = os.path.expanduser(source_db)
        source_db = os.path.abspath(source_db)
        
        if not os.path.exists(source_db):
            print(f"Error: Source database does not exist: {source_db}")
            continue

        # Generate proposed target filename with date_time
        source_dir = os.path.dirname(source_db)
        source_name = os.path.basename(source_db)
        base_name, ext = os.path.splitext(source_name)
        current_datetime = datetime.now().strftime("%Y%m%d_%H%M%S")  # Format: YYYYMMDD_HHMMSS
        proposed_name = f"60_{base_name}_{current_datetime}{ext}"
        proposed_path = os.path.join(source_dir, proposed_name)
        
        print(f"\nProposed target filename: {proposed_name}")
        print(f"Proposed full path: {proposed_path}")
        
        use_proposed = input("Use this path? (y/n, or enter new path): ").strip()
        
        if use_proposed.lower() == 'y':
            target_db = proposed_path
        else:
            target_db = use_proposed if use_proposed.lower() != 'n' else input("Enter the complete path for the target database: ").strip()
            
        target_db = os.path.expanduser(target_db)
        target_db = os.path.abspath(target_db)
            
        if not check_path_exists(target_db):
            print(f"Error: Target directory does not exist: {os.path.dirname(target_db)}")
            continue
            
        return source_db, target_db


def shrink_database(source_db: str, target_db: str, time_step: int = 60):
    """
    Shrink the MAC_UE table by selecting records with a specified time step.
    
    Args:
        source_db: Path to source database
        target_db: Path to target database file
        time_step: Time step in seconds (default 60 seconds = 1 minute)
    """
    try:
        print(f"\nShrinking database...")
        print(f"Source: {source_db}")
        print(f"Target: {target_db}")
        print(f"Time step: {time_step} seconds")

        # Connect to source database
        src_conn = sqlite3.connect(source_db)
        src_cur = src_conn.cursor()

        # Check if target exists
        if os.path.exists(target_db):
            user_input = input(f"\nTarget file already exists: {target_db}\nDo you want to replace it? (y/n): ")
            if user_input.lower() != 'y':
                print("Operation cancelled.")
                return
            os.remove(target_db)

        # Create new database
        tgt_conn = sqlite3.connect(target_db)
        tgt_cur = tgt_conn.cursor()

        # Copy schema
        src_cur.execute("SELECT sql FROM sqlite_master WHERE type='table' AND name='MAC_UE'")
        create_table_sql = src_cur.fetchone()[0]
        tgt_cur.execute(create_table_sql)

        # Get time range
        src_cur.execute("SELECT MIN(tstamp), MAX(tstamp) FROM MAC_UE")
        min_time_ms, max_time_ms = src_cur.fetchone()
        
        # Convert time_step to milliseconds
        time_step_ms = time_step * 1000

        # Efficient query to select one record per time step
        query = f"""
        WITH numbered AS (
            SELECT *,
                   ROW_NUMBER() OVER (
                       PARTITION BY CAST((tstamp - {min_time_ms}) / {time_step_ms} AS INT)
                       ORDER BY ABS(tstamp - (CAST((tstamp - {min_time_ms}) / {time_step_ms} AS INT) * {time_step_ms} + {min_time_ms}))
                   ) as rn
            FROM MAC_UE
        )
        SELECT * FROM numbered 
        WHERE rn = 1 
        ORDER BY tstamp
        """

        print("\nCopying data...")
        # Execute and copy data
        src_cur.execute(query)
        
        # Get column names
        columns = [description[0] for description in src_cur.description][:-1]  # Exclude 'rn' column
        columns_str = ', '.join(columns)
        placeholders = ','.join(['?' for _ in columns])
        
        # Insert data in chunks
        chunk_size = 1000
        total_rows = 0
        
        while True:
            rows = src_cur.fetchmany(chunk_size)
            if not rows:
                break
                
            # Remove the 'rn' column from the data
            rows = [row[:-1] for row in rows]
            
            tgt_cur.executemany(
                f"INSERT INTO MAC_UE ({columns_str}) VALUES ({placeholders})",
                rows
            )
            total_rows += len(rows)
            print(f"Processed {total_rows:,} records", end='\r')

        # Commit changes
        tgt_conn.commit()

        # Copy indices
        src_cur.execute("SELECT sql FROM sqlite_master WHERE type='index' AND tbl_name='MAC_UE'")
        for idx in src_cur.fetchall():
            if idx[0] is not None:  # Skip internal indices
                tgt_cur.execute(idx[0])
        tgt_conn.commit()

        # Get statistics
        src_cur.execute("SELECT COUNT(*) FROM MAC_UE")
        original_count = src_cur.fetchone()[0]
        tgt_cur.execute("SELECT COUNT(*) FROM MAC_UE")
        final_count = tgt_cur.fetchone()[0]

        print("\n\nDatabase shrinking completed!")
        print(f"Original records: {original_count:,}")
        print(f"Reduced records: {final_count:,}")
        print(f"Reduction ratio: {(1 - final_count/original_count)*100:.1f}%")
        print(f"New database size: {os.path.getsize(target_db)/1024/1024:.1f} MB")
        print(f"\nShrunk database saved as: {target_db}")

    except Exception as e:
        print(f"\nError: {str(e)}")
        raise

    finally:
        # Close connections
        try:
            src_cur.close()
            src_conn.close()
            tgt_cur.close()
            tgt_conn.close()
        except:
            pass

def main():
    print("Database Shrinking Tool")
    print("======================")
    
    # Get and validate paths
    source_db, target_db = get_valid_paths()
    
    # Shrink database with 1-minute step
    shrink_database(source_db, target_db, time_step=60)

if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\nOperation cancelled by user.")
    except Exception as e:
        print(f"\nAn error occurred: {str(e)}")
