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

#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <inttypes.h> // For PRIx32 macro
#include "../../../src/xApp/e42_xapp_api.h"
#include "../../../src/util/time_now_us.h"
#include "../../../external/common/utils/hashtable/hashtable.h"

#include "sm/gtp_sm/gtp_sm_id.h"
// #include "sm/gtp_sm/gtp_sm_common.h"
#include "sm/gtp_sm/ie/gtp_data_ie.h"
#include "sm/sm_proc_data.h"
#include "../../../src/util/alg_ds/alg/defer.h"

#include "../../../../RAN_FUNCTION/surrey_log.h"

// Add these includes at the top
#include "hiper_ran_tcp_client.h"

#define MAX_MOBILES_PER_GNB 40
#define NBR_DATA_COLLECTED 200

int count_collect_data = 0;

// Keep the necessary global variables
static pthread_mutex_t gtp_stats_mutex;
static pthread_mutex_t mac_stats_mutex;
// tcp client thread
static pthread_t tcp_client_thread_id;

hash_table_t* ue_gtp_stats_by_rnti_ht;
hash_table_t* ue_mac_stats_by_rnti_ht;

static sm_ans_xapp_t* gtp_handle = NULL;
// MAC indication
static sm_ans_xapp_t* mac_handle = NULL;
static e2_node_arr_xapp_t nodes;

static volatile sig_atomic_t keepRunning = 1;

// static uint64_t cnt_gtp = 0;

void intHandler()
{
  keepRunning = false;
}

// Function to safely remove service model handlers
static void remove_service_handlers(void)
{
  // Lock both mutexes to ensure safe cleanup
  pthread_mutex_lock(&gtp_stats_mutex);
  pthread_mutex_lock(&mac_stats_mutex);

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
  }
  // Give some time for handlers to complete
  usleep(100000); // 100ms
  pthread_mutex_unlock(&mac_stats_mutex);
  pthread_mutex_unlock(&gtp_stats_mutex);
}

// Cleanup function simplified for GTP only
void cleanup_all_resources()
{
  printf("Starting comprehensive cleanup...\n");

  // 1. Stop TCP client
  if (tcp_client_thread_id != 0) {
    printf("Cleaning up TCP client...\n");
    cleanup_tcp_client();
    pthread_join(tcp_client_thread_id, NULL);
    tcp_client_thread_id = 0;
  }

  // 2. Remove service handlers
  printf("Removing service handlers...\n");
  remove_service_handlers();

  // 3. Free service handles
  if (nodes.len > 0) {
    if (gtp_handle) {
      free(gtp_handle);
      gtp_handle = NULL;
    }
    if (mac_handle) {
      free(mac_handle);
      mac_handle = NULL;
    }
  }

  // 4. Clean up hash tables
  printf("Cleaning up hash tables...\n");
  if (ue_gtp_stats_by_rnti_ht) {
    hashtable_destroy(&ue_gtp_stats_by_rnti_ht);
    ue_gtp_stats_by_rnti_ht = NULL;
  }
  if (ue_mac_stats_by_rnti_ht) {
    hashtable_destroy(&ue_mac_stats_by_rnti_ht);
    ue_mac_stats_by_rnti_ht = NULL;
  }

  // 5. Destroy service model mutexes
  printf("Destroying mutexes...\n");
  pthread_mutex_destroy(&gtp_stats_mutex);
  pthread_mutex_destroy(&mac_stats_mutex);

  // 6. Free E2 nodes array
  if (nodes.len > 0) {
    printf("Freeing E2 nodes array...\n");
    free_e2_node_arr_xapp(&nodes);
  }
}

// Signal handler function
static void signal_handler(int signum)
{
  if (signum == SIGINT || signum == SIGTERM) {
    printf("\nReceived signal %d. Initiating graceful shutdown...\n", signum);
    keepRunning = 0;

    // Remove service handlers
    printf("Removing service handlers...\n");
    remove_service_handlers();

    // Set client running flag to false and close socket
    force_close_socket(); // Using the function from tcp client

    // Clean up resources
    cleanup_all_resources();

    // Let the core API handle the final exit
    printf("Local cleanup complete, proceeding to core shutdown...\n");
  }
}

static void sm_cb_gtp(sm_ag_if_rd_t const* rd)
{
  assert(rd != NULL);
  assert(rd->ind.type == GTP_STATS_V0);
  // bool cond_send_ho_ok = false;

  char buffer[8191] = {0};

  pthread_mutex_lock(&gtp_stats_mutex);

  int64_t now = time_now_us();

  // printf("\n=== Decoded GTP Stats ===\n");

  for (u_int32_t i = 0; i < rd->ind.gtp.msg.len; i++) {
    sprintf(buffer,
            "{\"timestamp\": %ju, \"rnti_gtp\": %04x,\"teid_gnb\": %04x,\"teid_upf\": %d,\"qfi\": %d,\"ue_has_mqr\": %s, "
            "\"rrc_ue_id\": %d,\"ue_rnti_t\": %d,\"mqr_rsrp\": %ld,\"mqr_rsrq\": %.1f,\"mqr_sinr\": %.1f,"
            "\"ho_ue_id\": %d, \"ho_complete\": %d, \"ho_source_du\": %d, \"ho_target_du\": %d",
            now / 1000,
            rd->ind.gtp.msg.ngut[i].rnti,
            rd->ind.gtp.msg.ngut[i].teidgnb,
            rd->ind.gtp.msg.ngut[i].teidupf,
            rd->ind.gtp.msg.ngut[i].qfi,
            (rd->ind.gtp.msg.ngut[i].ue_context_has_mqr == 1) ? "true" : "false",
            rd->ind.gtp.msg.ngut[i].ue_context_rrc_ue_id,
            rd->ind.gtp.msg.ngut[i].ue_context_rnti_t,
            rd->ind.gtp.msg.ngut[i].ue_context_mqr_rsrp,
            rd->ind.gtp.msg.ngut[i].ue_context_mqr_rsrq,
            rd->ind.gtp.msg.ngut[i].ue_context_mqr_sinr,
            rd->ind.gtp.msg.ho_info.ue_id,
            (rd->ind.gtp.msg.ho_info.ho_complete) ? 1 : 0,
            rd->ind.gtp.msg.ho_info.source_du,
            rd->ind.gtp.msg.ho_info.target_du);

    printf("count: %d - GTP INFO: \n%s\n", count_collect_data, buffer);
    fflush(stdout);
    //   exit(0);
  }
  count_collect_data++;

  if (count_collect_data == NBR_DATA_COLLECTED) {
    // if (rd->ind.gtp.msg.ho_info.ho_complete == 1) {
    //   Send message to the RU controler that DU Handover OK
    send_ho_ok();
    count_collect_data = 0;
    LOG_SURREY("Handover OK sent to switch off the RU controller/ Source DU: %d and Target DU: %d \n",
               rd->ind.gtp.msg.ho_info.source_du,
               rd->ind.gtp.msg.ho_info.target_du);
    // cond_send_ho_ok = true;
  }
  pthread_mutex_unlock(&gtp_stats_mutex);
}

void write_mac_stats(char* buffer, mac_ue_stats_impl_t* ue_mac_stats)
{
  sprintf(&buffer[strlen(buffer)],
          "\"MAC:\",\"cellid\": %lu,\"in_sync\": %d, \"rnti\": \"%04x\",\"dlBytes\": %lu,\"dlMcs\": %d,\"dlBler\": "
          "%f,\"ulBytes\": %ld,"
          "\"ulMcs\": %d,\"ulBler\": %f,\"ri\": %d,\"pmi\": \"(%d,%d)\",\"phr\": %d,\"pcmax\": %d,",
          ue_mac_stats->nr_cellid,
          ue_mac_stats->in_sync,
          ue_mac_stats->rnti,
          ue_mac_stats->dl_aggr_tbs,
          ue_mac_stats->dl_mcs1,
          ue_mac_stats->dl_bler,
          ue_mac_stats->ul_aggr_tbs,
          ue_mac_stats->ul_mcs1,
          ue_mac_stats->ul_bler,
          ue_mac_stats->pmi_cqi_ri,
          ue_mac_stats->pmi_cqi_X1,
          ue_mac_stats->pmi_cqi_X2,
          ue_mac_stats->phr,
          ue_mac_stats->pcmax);
  printf("MAC INFO:\n%s\n", buffer);
  fflush(stdout);
}

static void sm_cb_mac(sm_ag_if_rd_t const* rd)
{
  assert(rd != NULL);
  assert(rd->type == INDICATION_MSG_AGENT_IF_ANS_V0);
  assert(rd->ind.type == MAC_STATS_V0);

  // int64_t now = time_now_us();
  //  if (cnt_mac % 1024 == 0)
  //    printf("MAC ind_msg latency = %ld μs\n", now - rd->ind.mac.msg.tstamp);
  //  // cnt_mac++;
  for (u_int32_t i = 0; i < rd->ind.mac.msg.len_ue_stats; i++) {
    pthread_mutex_lock(&mac_stats_mutex);
    // printf("MAC ind_msg latency = %ld μs\n", now - rd->ind.mac.msg.tstamp);
    mac_ue_stats_impl_t* ue_mac_stats = malloc(sizeof(*ue_mac_stats));
    *ue_mac_stats = rd->ind.mac.msg.ue_stats[i];
    hashtable_insert(ue_mac_stats_by_rnti_ht, rd->ind.mac.msg.ue_stats[i].rnti, ue_mac_stats);

    // Check if the insertion was successful
    if (ue_mac_stats_by_rnti_ht != NULL) {
      // Hashtable is not empty
      // printf("Hash not empty at least one\n");
      int64_t now = time_now_us();

      if (!ue_mac_stats->in_sync) {
        printf("UE: %04x is out of sync, removing stats hashtable entry\n", ue_mac_stats->rnti);
        hashtable_remove(ue_mac_stats_by_rnti_ht, ue_mac_stats->rnti);
        continue;
      }
      char buffer[8191] = {0};
      sprintf(buffer, "{\"timestamp\": %ju,", now / 1000);

      write_mac_stats(buffer, ue_mac_stats);

      double rsrq = NAN;
      double sinr = NAN;
      long rsrp = ue_mac_stats->rsrp;
      uint8_t cqi = ue_mac_stats->cqi;
      float rssi = -1 * ue_mac_stats->raw_rssi;
      float pucch_snr = ue_mac_stats->pucch_snr;
      float pusch_snr = ue_mac_stats->pusch_snr;

      if (!isnan(rsrq)) {
        sprintf(&buffer[strlen(buffer)], "\"rsrq\": %.1f, ", rsrq);
      }

      if (!isnan(sinr)) {
        sprintf(&buffer[strlen(buffer)], "\"sinr\": %.1f, ", sinr);
      }

      sprintf(&buffer[strlen(buffer)],
              "\"rsrp\": %ld,\"rssi\": %.1f,\"cqi\": %d,\"pucchSnr\": %.1f,\"puschSnr\": %.1f",
              rsrp,
              rssi,
              cqi,
              pucch_snr,
              pusch_snr);

      sprintf(&buffer[strlen(buffer)], "}\n");
      // Print the contents of the buffer
      printf("Measured MAC KPI:\n%s\n", buffer);
    } else {
      printf("Hash is empty\n");
      // Hashtable is empty, insertion might have failed
    }
    pthread_mutex_unlock(&mac_stats_mutex);
  }
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
  // Initialize flexRIC arguments from command line
  fr_args_t args = init_fr_args(argc, argv);

  // Initialize mutex attributes and mutex for GTP only
  pthread_mutexattr_t attr = {0};
  pthread_mutex_init(&gtp_stats_mutex, &attr);
  pthread_mutex_init(&mac_stats_mutex, &attr);

  // Initialize hash table for GTP statistics tracking
  ue_gtp_stats_by_rnti_ht = hashtable_create(MAX_MOBILES_PER_GNB, NULL, free);
  ue_mac_stats_by_rnti_ht = hashtable_create(MAX_MOBILES_PER_GNB, NULL, free);

  // Init the xApp
  init_xapp_api(&args);
  sleep(1);

  // Start the TCP client thread
  tcp_client_thread_id = start_tcp_client();
  if (tcp_client_thread_id == 0) {
    printf("Failed to start TCP client thread\n");
    // Handle error
    goto cleanup;
  }

  // Get connected E2 nodes
  // Global variable
  nodes = e2_nodes_xapp_api();

  // Verify at least one node is connected
  assert(nodes.len > 0);
  printf("Connected E2 nodes = %d\n", nodes.len);

  // Define reporting interval for GTP
  // Allowed 1_ms, 2_ms, 5_ms, 10_ms, 100_ms and 1000_ms
  const char* i_ms = "100_ms";

  if (nodes.len > 0) {
    gtp_handle = calloc(nodes.len, sizeof(sm_ans_xapp_t));
    assert(gtp_handle != NULL);
    mac_handle = calloc(nodes.len, sizeof(sm_ans_xapp_t));
    assert(mac_handle != NULL);
  }

  // Setup GTP and MAC reporting for each node
  for (int i = 0; i < nodes.len; i++) {
    e2_node_connected_xapp_t* n = &nodes.n[i];

    for (size_t j = 0; j < n->len_rf; j++)
      printf("Registered node %d ran func id = %d \n ", i, n->rf[j].id);

    // Setup GTP for appropriate node types
    if (n->id.type == ngran_gNB || n->id.type == ngran_eNB) {
      gtp_handle[i] = report_sm_xapp_api(&nodes.n[i].id, 148, (void*)i_ms, sm_cb_gtp);
      assert(gtp_handle[i].success == true);
      mac_handle[i] = report_sm_xapp_api(&nodes.n[i].id, 142, (void*)i_ms, sm_cb_mac);
      assert(mac_handle[i].success == true);
    } else {
      if (n->id.type == ngran_gNB_CU || n->id.type == ngran_gNB_CUUP) {
        gtp_handle[i] = report_sm_xapp_api(&nodes.n[i].id, 148, (void*)i_ms, sm_cb_gtp);
        assert(gtp_handle[i].success == true);
      } else {
        if (n->id.type == ngran_gNB_DU) {
          mac_handle[i] = report_sm_xapp_api(&nodes.n[i].id, 142, (void*)i_ms, sm_cb_mac);
          assert(mac_handle[i].success == true);
        }
      }
    }
  }

  // Main loop
  while (keepRunning) {
    usleep(100000); // Sleep for 100ms
  }
cleanup:
  // Before the program exits (before cleanup_all_resources call)
  // Cleanup TCP client before other resources

  printf("\nInitiating shutdown sequence...\n");
  cleanup_all_resources();
  printf("Application terminated successfully\n");
  return 0;
}
