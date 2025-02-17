#include "db_metrics.h"
#include <stdio.h>
#include <time.h>
#include <string.h>

static sqlite3* db = NULL;

static const int NUM_SCHEMAS = sizeof(SCHEMA_DEFINITIONS) / sizeof(SCHEMA_DEFINITIONS[0]);

bool init_metrics_db(const char* db_path)
{
  if (db != NULL) {
    return true; // Already initialized
  }

  int rc = sqlite3_open(db_path, &db);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
    return false;
  }

  char* err_msg = NULL;
  for (int i = 0; i < NUM_SCHEMAS; i++) {
    rc = sqlite3_exec(db, SCHEMA_DEFINITIONS[i], NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
      fprintf(stderr, "SQL error: %s\n", err_msg);
      sqlite3_free(err_msg);
      return false;
    }
  }

  return true;
}

// Updated schema definitions
static const char* SCHEMA_DEFINITIONS[] = {
    // Latencies table
    "CREATE TABLE IF NOT EXISTS latencies ("
    "id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "timestamp INTEGER NOT NULL," // measurement_timestamp
    "time_str TEXT NOT NULL," // Added time string field
    "counter INTEGER NOT NULL,"
    "latency INTEGER NOT NULL," // in microseconds
    "num_nodes INTEGER NOT NULL"
    ");",

    // UE measurements table
    "CREATE TABLE IF NOT EXISTS ue_measurements ("
    "id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "timestamp INTEGER NOT NULL," // measurement_timestamp
    "time_str TEXT NOT NULL," // Added time string field
    "counter INTEGER NOT NULL,"
    "ue_id INTEGER NOT NULL,"
    "num_measurements INTEGER NOT NULL,"
    "FOREIGN KEY(counter) REFERENCES latencies(counter)"
    ");"};

void store_detailed_metrics(const kpm_ind_data_t* ind,
                            int64_t latency, // latency in microseconds
                            int counter,
                            const e2_node_arr_xapp_t* nodes,
                            int64_t measurement_timestamp,
                            char* time_str)
{
  if (!db) {
    fprintf(stderr, "Database not initialized\n");
    return;
  }

  // Begin transaction
  char* err_msg = NULL;
  if (sqlite3_exec(db, "BEGIN TRANSACTION", NULL, NULL, &err_msg) != SQLITE_OK) {
    fprintf(stderr, "Failed to begin transaction: %s\n", err_msg);
    sqlite3_free(err_msg);
    return;
  }

  // 1. Insert into latencies table
  sqlite3_stmt* stmt;
  const char* sql_latencies =
      "INSERT INTO latencies "
      "(timestamp, time_str, counter, latency, num_nodes) "
      "VALUES (?, ?, ?, ?, ?);";

  bool success = true;

  if (sqlite3_prepare_v2(db, sql_latencies, -1, &stmt, 0) == SQLITE_OK) {
    sqlite3_bind_int64(stmt, 1, measurement_timestamp);
    sqlite3_bind_text(stmt, 2, time_str, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 3, counter);
    sqlite3_bind_int64(stmt, 4, latency);
    sqlite3_bind_int(stmt, 5, nodes->len);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
      fprintf(stderr, "Failed to insert latency: %s\n", sqlite3_errmsg(db));
      success = false;
    }
    sqlite3_finalize(stmt);
  }

  // 2. Insert into ue_measurements table
  if (success && ind->msg.frm_3.ue_meas_report_lst_len > 0) {
    const char* sql_ue =
        "INSERT INTO ue_measurements "
        "(timestamp, time_str, counter, ue_id, num_measurements) "
        "VALUES (?, ?, ?, ?, ?);";

    const kpm_ind_msg_format_3_t* msg_frm_3 = &ind->msg.frm_3;

    for (size_t i = 0; i < msg_frm_3->ue_meas_report_lst_len && success; i++) {
      const ue_id_e2sm_t* ue_id = &msg_frm_3->meas_report_per_ue[i].ue_meas_report_lst;
      const kpm_ind_msg_format_1_t* measurements = &msg_frm_3->meas_report_per_ue[i].ind_msg_format_1;

      if (sqlite3_prepare_v2(db, sql_ue, -1, &stmt, 0) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, measurement_timestamp);
        sqlite3_bind_text(stmt, 2, time_str, -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 3, counter);
        sqlite3_bind_int(stmt, 4, ue_id->type);
        sqlite3_bind_int(stmt, 5, measurements->meas_data_lst_len);

        if (sqlite3_step(stmt) != SQLITE_DONE) {
          fprintf(stderr, "Failed to insert UE measurement: %s\n", sqlite3_errmsg(db));
          success = false;
        }
        sqlite3_finalize(stmt);
      }
    }
  }

  // Commit or rollback transaction
  if (success) {
    if (sqlite3_exec(db, "COMMIT", NULL, NULL, &err_msg) != SQLITE_OK) {
      fprintf(stderr, "Failed to commit transaction: %s\n", err_msg);
      sqlite3_free(err_msg);
      sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
    }
  } else {
    sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
  }
}

void close_metrics_db(void)
{
  if (db != NULL) {
    // Attempt to close the database connection
    int rc = sqlite3_close(db);

    if (rc != SQLITE_OK) {
      // If there's an error closing the database, print the error
      fprintf(stderr, "Error closing database: %s\n", sqlite3_errmsg(db));
    } else {
      // Successfully closed
      db = NULL;
    }
  }
}
