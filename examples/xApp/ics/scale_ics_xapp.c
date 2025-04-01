/*
 * File: ics_xapp_gtp_mac_rlc_pdcp_moni_extend.c
 * Author: Rafik ZITOUNI
 * Date: 2024-11-08
 *
 * License: MIT License
 *
 * Copyright (c) 2024 Rafik ZITOUNI
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * ...
 */
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>

#include <math.h>
#include <netdb.h>
#include <sys/socket.h>

#include <sqlite3.h>
#include <getopt.h>

#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <inttypes.h> // For PRIx32 macro
#include "../../../src/xApp/e42_xapp_api.h"
#include "../../../src/util/time_now_us.h"
#include "../../../../../../common/utils/hashtable/hashtable.h"

#include "sm/gtp_sm/gtp_sm_id.h"
// #include "sm/gtp_sm/gtp_sm_common.h"
#include "sm/gtp_sm/ie/gtp_data_ie.h"
#include "sm/sm_proc_data.h"
#include "../../../src/util/alg_ds/alg/defer.h"

#include "../../../../RAN_FUNCTION/surrey_log.h"

#define MAX_MOBILES_PER_GNB 40
// #define NBR_DATA_COLLECTED 200

// int count_collect_data = 0;

// Keep the necessary global variables
static pthread_mutex_t gtp_stats_mutex;
static pthread_mutex_t mac_stats_mutex;
static pthread_mutex_t pdcp_stats_mutex;
static pthread_mutex_t rlc_stats_mutex;

static char* g_selected_sm = NULL; // parser of connand SM option

// GTP indication
static sm_ans_xapp_t* gtp_handle = NULL;
// MAC indication
static sm_ans_xapp_t* mac_handle = NULL;
// PDCP indication
static sm_ans_xapp_t* pdcp_handle = NULL;
// RLC indication
static sm_ans_xapp_t* rlc_handle = NULL;

static e2_node_arr_xapp_t nodes;

static volatile sig_atomic_t keepRunning = 1;

static sqlite3* db = NULL;
static char db_path[512];

// static uint64_t cnt_gtp = 0;

void intHandler()
{
  keepRunning = false;
}

// Initialize database connection
int init_database(const char* db_name)
{
  snprintf(db_path, sizeof(db_path), "/var/lib/grafana/%s", db_name);

  int rc = sqlite3_open_v2(db_path, &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, NULL);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "Can't open database at %s: %s\n", db_path, sqlite3_errmsg(db));
    return rc;
  }

  // char* err_msg = 0;
  // rc = sqlite3_exec(db, sql, 0, 0, &err_msg);

  // Enable WAL mode
  char* err_msg = 0;

  // Set busy timeout to 5 seconds
  sqlite3_exec(db, "PRAGMA busy_timeout = 5000;", NULL, NULL, NULL);

  // Enable WAL mode BEFORE creating the table
  rc = sqlite3_exec(db, "PRAGMA journal_mode=WAL;", NULL, NULL, &err_msg);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "Failed to enable WAL mode: %s\n", err_msg);
    sqlite3_free(err_msg);
    sqlite3_close(db);
    return rc;
  }

  // Create table with correct schema
  const char* sql =
      "CREATE TABLE IF NOT EXISTS metrics ("
      "id INTEGER PRIMARY KEY AUTOINCREMENT,"
      "sm TEXT NOT NULL,"
      "tstamp INTEGER NOT NULL,"
      "latency INTEGER NOT NULL,"
      "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP)";

  rc = sqlite3_exec(db, sql, 0, 0, &err_msg);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "SQL error: %s\n", err_msg);
    sqlite3_free(err_msg);
    sqlite3_close(db);
    return rc;
  }

  LOG_SURREY("RIC Scale Latency Database initialized successfully at %s\n", db_path);
  return SQLITE_OK;
}
// Function to save metrics to database
void save_metrics(const char* sm, int64_t tstamp, int64_t latency)
{
  static int row_count = 0; // Static counter to track insertions

  if (!db) {
    fprintf(stderr, "Database not initialized\n");
    return;
  }

  // Prepare the INSERT statement
  sqlite3_stmt* stmt = NULL;
  const char* sql = "INSERT INTO metrics (sm, tstamp, latency) VALUES (?, ?, ?);";

  int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
    return;
  }

  // Bind values to prevent SQL injection and ensure proper insertion
  sqlite3_bind_text(stmt, 1, sm, -1, SQLITE_STATIC);
  sqlite3_bind_int64(stmt, 2, tstamp);
  sqlite3_bind_int64(stmt, 3, latency);

  // Execute the statement
  rc = sqlite3_step(stmt);
  if (rc != SQLITE_DONE) {
    fprintf(stderr, "Failed to insert row: %s\n", sqlite3_errmsg(db));
  } else {
    row_count++;
  }

  // Finalize the statement
  sqlite3_finalize(stmt);
}

// Function to safely remove service model handlers
static void remove_service_handlers(void)
{
  // Lock both mutexes to ensure safe cleanup
  pthread_mutex_lock(&gtp_stats_mutex);
  pthread_mutex_lock(&mac_stats_mutex);
  pthread_mutex_lock(&pdcp_stats_mutex);
  pthread_mutex_lock(&rlc_stats_mutex);

  for (int i = 0; i < nodes.len; i++) {
    if (gtp_handle && gtp_handle[i].u.handle != 0) {
      printf("Removing GTP handler for node %d\n", i);
      rm_report_sm_xapp_api(gtp_handle[i].u.handle);
      gtp_handle[i].u.handle = 0;
    }
    if (mac_handle && mac_handle[i].u.handle != 0) {
      printf("Removing MAC handler for node %d\n", i);
      rm_report_sm_xapp_api(mac_handle[i].u.handle);
      mac_handle[i].u.handle = 0;
    }
    if (pdcp_handle && pdcp_handle[i].u.handle != 0) {
      printf("Removing PDCP handler for node %d\n", i);
      rm_report_sm_xapp_api(pdcp_handle[i].u.handle);
      pdcp_handle[i].u.handle = 0;
    }
    if (rlc_handle && rlc_handle[i].u.handle != 0) {
      printf("Removing RLC handler for node %d\n", i);
      rm_report_sm_xapp_api(rlc_handle[i].u.handle);
      rlc_handle[i].u.handle = 0;
    }
  }
  // Give some time for handlers to complete
  usleep(100000); // 100ms
  pthread_mutex_unlock(&rlc_stats_mutex);
  pthread_mutex_unlock(&pdcp_stats_mutex);
  pthread_mutex_unlock(&mac_stats_mutex);
  pthread_mutex_unlock(&gtp_stats_mutex);
}

// Cleanup function simplified for GTP only
void cleanup_all_resources()
{
  printf("Starting comprehensive cleanup...\n");

  // cleanup db
  if (db) {
    sqlite3_close(db);
    db = NULL;
  }
  // 1. Remove service handlers
  printf("Removing service handlers...\n");
  remove_service_handlers();

  if (g_selected_sm) {
    free(g_selected_sm);
    g_selected_sm = NULL;
  }

  // 2. Free service handles
  if (nodes.len > 0) {
    if (gtp_handle) {
      free(gtp_handle);
      gtp_handle = NULL;
    }
    if (mac_handle) {
      free(mac_handle);
      mac_handle = NULL;
    }
    if (pdcp_handle) {
      free(pdcp_handle);
      pdcp_handle = NULL;
    }
    if (rlc_handle) {
      free(rlc_handle);
      rlc_handle = NULL;
    }
  }

  // 3. Destroy service model mutexes
  printf("Destroying mutexes...\n");
  pthread_mutex_destroy(&gtp_stats_mutex);
  pthread_mutex_destroy(&mac_stats_mutex);
  pthread_mutex_destroy(&pdcp_stats_mutex);
  pthread_mutex_destroy(&rlc_stats_mutex);
  // 4. Free E2 nodes array
  if (nodes.len > 0) {
    printf("Freeing E2 nodes array...\n");
    free_e2_node_arr_xapp(&nodes);
  }
}

// Signal handler function
static void signal_handler(int signum)
{
  if (signum == SIGINT || signum == SIGTERM) {
    LOG_SURREY("\nReceived signal %d. Initiating graceful shutdown...\n", signum);
    keepRunning = 0;

    // Remove service handlers
    printf("Removing service handlers...\n");
    remove_service_handlers();

    // Clean up resources
    cleanup_all_resources();

    // Let the core API handle the final exit
    printf("Local cleanup complete, proceeding to core shutdown...\n");
  }
}

void sm_cb_mac(sm_ag_if_rd_t const* src)
{
  pthread_mutex_lock(&mac_stats_mutex);
  // printf("[ICS xApp]: Length of the UE Stats: %d\n", src->ind.mac.msg.len_ue_stats);
  int64_t now = time_now_us();
  int64_t latency = now - src->ind.mac.msg.tstamp;
  printf("[MAC SM]: MAC Timestamp: %ld, latency: %ld, UE RNTI: %X, UE WB CQI: %d, PSCH SNR: %.1f, \n",
         src->ind.mac.msg.tstamp,
         latency,
         src->ind.mac.msg.ue_stats->rnti,
         src->ind.mac.msg.ue_stats->wb_cqi,
         src->ind.mac.msg.ue_stats->pusch_snr);

  save_metrics("MAC", now, latency);
  pthread_mutex_unlock(&mac_stats_mutex);
}

void sm_cb_gtp(sm_ag_if_rd_t const* src)
{
  pthread_mutex_lock(&gtp_stats_mutex);
  int64_t now = time_now_us();
  int64_t latency = now - src->ind.gtp.msg.tstamp;
  // printf("[GTP SM]: Length of the UE Stats: %d\n", src->ind.mac.msg.len_ue_stats);
  printf("[GTP SM]: GTP Timestamp: %ld, latency: %ld, TEID UPF: %X \n",
         src->ind.gtp.msg.tstamp,
         latency,
         src->ind.gtp.msg.ngut[0].teidupf);
  save_metrics("GTP", now, latency);

  pthread_mutex_unlock(&gtp_stats_mutex);
}

void sm_cb_pdcp(sm_ag_if_rd_t const* src)
{
  pthread_mutex_lock(&pdcp_stats_mutex);
  int64_t now = time_now_us();
  int64_t latency = now - src->ind.pdcp.msg.tstamp;
  printf("[PDCP SM]: PDCP Timestamp: %ld, latency: %ld, RB ID: %X, \n",
         src->ind.pdcp.msg.tstamp,
         latency,
         src->ind.pdcp.msg.rb->rbid);
  save_metrics("PDCP", now, latency);
  pthread_mutex_unlock(&pdcp_stats_mutex);
}

void sm_cb_rlc(sm_ag_if_rd_t const* src)
{
  pthread_mutex_lock(&rlc_stats_mutex);
  int64_t now = time_now_us();
  int64_t latency = now - src->ind.rlc.msg.tstamp;
  printf("[RLC SM]: RLC Timestamp: %ld, latency: %ld, RB ID: %X, \n", src->ind.rlc.msg.tstamp, latency, src->ind.rlc.msg.rb->rbid);
  save_metrics("RLC", now, latency);
  pthread_mutex_unlock(&rlc_stats_mutex);
}

// Function to handle database initialization from command line arguments
int handle_extra_option(int argc, char* argv[], int* new_argc, char*** new_argv)
{
  char* db_name = NULL;
  char* sm_type = NULL;
  optind = 1; // Reset getopt index

  // Create new argument array
  char** filtered_argv = malloc(argc * sizeof(char*));
  if (filtered_argv == NULL) {
    fprintf(stderr, "Memory allocation failed\n");
    return -1;
  }
  int filtered_argc = 0;

  // Always keep the program name
  filtered_argv[filtered_argc] = strdup(argv[0]);
  if (filtered_argv[filtered_argc] == NULL) {
    free(filtered_argv);
    fprintf(stderr, "Memory allocation failed\n");
    return -1;
  }
  filtered_argc++;

  // Copy arguments, skipping -db and its value
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-db") == 0) {
      if (i + 1 < argc) {
        db_name = argv[i + 1];
        i++; // Skip the database name
      }
      continue;
    } else if (strcmp(argv[i], "-sm") == 0) {
      if (i + 1 < argc) {
        sm_type = argv[i + 1];
        // Validate SM type
        if (strcmp(sm_type, "mac") != 0 && strcmp(sm_type, "gtp") != 0 && strcmp(sm_type, "rlc") != 0
            && strcmp(sm_type, "pdcp") != 0 && strcmp(sm_type, "all") != 0) {
          fprintf(stderr, "Error: Invalid SM type. Valid types are: mac, gtp, rlc, pdcp\n");
          goto cleanup;
        }
        i++; // Skip the SM type
      }
      continue;
    }
    filtered_argv[filtered_argc] = strdup(argv[i]);
    if (filtered_argv[filtered_argc] == NULL) {
      goto cleanup;
    }
    filtered_argc++;
  }

  // Validate database name
  if (db_name == NULL) {
    fprintf(stderr, "Error: Database name (-db) is required\n");
    goto cleanup;
  }

  if (sm_type == NULL) {
    fprintf(stderr, "Error: SM type (-sm) is required. Valid types are: mac, gtp, rlc, pdcp\n");
    goto cleanup;
  }
  // Store SM type in a global variable or structure for later use
  g_selected_sm = strdup(sm_type);
  if (g_selected_sm == NULL) {
    fprintf(stderr, "Error: Failed to store SM type\n");
    goto cleanup;
  }

  // Initialize database
  if (init_database(db_name) != SQLITE_OK) {
    fprintf(stderr, "Failed to initialize database\n");
    goto cleanup;
  }

  *new_argc = filtered_argc;
  *new_argv = filtered_argv;
  return 0;

cleanup:
  for (int i = 0; i < filtered_argc; i++) {
    free(filtered_argv[i]);
  }
  free(filtered_argv);
  return -1;
}

int main(int argc, char* argv[])
{
  // Set up signal handling
  struct sigaction sa = {0};
  sa.sa_handler = signal_handler;
  sigemptyset(&sa.sa_mask);
  sigfillset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;

  if (sigaction(SIGINT, &sa, NULL) == -1 || sigaction(SIGTERM, &sa, NULL) == -1) {
    perror("Failed to set up signal handlers");
    return 1;
  }
  // Handle database initialization
  int new_argc;
  char** new_argv;
  if (handle_extra_option(argc, argv, &new_argc, &new_argv) != 0) {
    return 1;
  }
  // Initialize flexRIC arguments from command line
  fr_args_t args = init_fr_args(new_argc, new_argv);

  // Free the filtered argument array when done
  for (int i = 0; i < new_argc; i++) {
    free(new_argv[i]);
  }
  free(new_argv);

  // Initialize mutex attributes and mutex for GTP only
  pthread_mutexattr_t attr = {0};
  pthread_mutex_init(&gtp_stats_mutex, &attr);
  pthread_mutex_init(&mac_stats_mutex, &attr);
  pthread_mutex_init(&pdcp_stats_mutex, &attr);
  pthread_mutex_init(&rlc_stats_mutex, &attr);

  // Init the xApp
  init_xapp_api(&args);
  sleep(1);

  // Get connected E2 nodes
  // Global variable
  nodes = e2_nodes_xapp_api();

  // Verify at least one node is connected
  assert(nodes.len > 0);
  printf("Connected E2 nodes = %d\n", nodes.len);

  // Define reporting interval for GTP
  // Allowed 1_ms, 2_ms, 5_ms, 10_ms, 100_ms and 1000_ms
  const char* i_ms = "1_ms";

  if (nodes.len > 0) {
    gtp_handle = calloc(nodes.len, sizeof(sm_ans_xapp_t));
    assert(gtp_handle != NULL);
    mac_handle = calloc(nodes.len, sizeof(sm_ans_xapp_t));
    assert(mac_handle != NULL);
    pdcp_handle = calloc(nodes.len, sizeof(sm_ans_xapp_t));
    assert(pdcp_handle != NULL);
    rlc_handle = calloc(nodes.len, sizeof(sm_ans_xapp_t));
    assert(rlc_handle != NULL);
  }

  // Setup GTP and MAC reporting for each node
  for (int i = 0; i < nodes.len; i++) {
    e2_node_connected_xapp_t* n = &nodes.n[i];

    for (size_t j = 0; j < n->len_rf; j++)
      printf("Registered node %d ran func id = %d \n ", i, n->rf[j].id);

    // Setup GTP for appropriate node types
    if (n->id.type == ngran_gNB || n->id.type == ngran_eNB) {
      if (g_selected_sm && strcmp(g_selected_sm, "all") == 0) {
        gtp_handle[i] = report_sm_xapp_api(&nodes.n[i].id, 148, (void*)i_ms, sm_cb_gtp);
        assert(gtp_handle[i].success == true);
        mac_handle[i] = report_sm_xapp_api(&nodes.n[i].id, 142, (void*)i_ms, sm_cb_mac);
        assert(mac_handle[i].success == true);
        pdcp_handle[i] = report_sm_xapp_api(&nodes.n[i].id, 144, (void*)i_ms, sm_cb_pdcp);
        assert(pdcp_handle[i].success == true);
        rlc_handle[i] = report_sm_xapp_api(&nodes.n[i].id, 143, (void*)i_ms, sm_cb_rlc);
        assert(rlc_handle[i].success == true);
      }
      if (g_selected_sm && strcmp(g_selected_sm, "mac") == 0) {
        mac_handle[i] = report_sm_xapp_api(&nodes.n[i].id, 142, (void*)i_ms, sm_cb_mac);
        assert(mac_handle[i].success == true);
      }
      if (g_selected_sm && strcmp(g_selected_sm, "gtp") == 0) {
        gtp_handle[i] = report_sm_xapp_api(&nodes.n[i].id, 148, (void*)i_ms, sm_cb_gtp);
        assert(gtp_handle[i].success == true);
      }
      if (g_selected_sm && strcmp(g_selected_sm, "pdcp") == 0) {
        pdcp_handle[i] = report_sm_xapp_api(&nodes.n[i].id, 144, (void*)i_ms, sm_cb_pdcp);
        assert(pdcp_handle[i].success == true);
      }
      if (g_selected_sm && strcmp(g_selected_sm, "rlc") == 0) {
        rlc_handle[i] = report_sm_xapp_api(&nodes.n[i].id, 143, (void*)i_ms, sm_cb_rlc);
        assert(rlc_handle[i].success == true);
      }
    }
  }

  // Main loop
  while (keepRunning) {
    usleep(400000); // Sleep for 200ms
  }
  // Before the program exits (before cleanup_all_resources call)

  printf("\nInitiating shutdown sequence...\n");
  cleanup_all_resources();
  printf("Application terminated successfully\n");
  return 0;
}
