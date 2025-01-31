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

#define MAX_MOBILES_PER_GNB 40

// Keep the necessary global variables
static pthread_mutex_t gtp_stats_mutex;
static hash_table_t* ue_gtp_stats_by_rnti_ht;

static volatile sig_atomic_t keepRunning = 1;

// static uint64_t cnt_gtp = 0;

void intHandler()
{
  keepRunning = false;
}

void setup_signal_handler()
{
  struct sigaction act;
  memset(&act, 0, sizeof(act));
  act.sa_handler = intHandler;
  sigaction(SIGINT, &act, NULL);
  sigaction(SIGTERM, &act, NULL);
}

// Cleanup function simplified for GTP only
void cleanup_resources(sm_ans_xapp_t* gtp_handle, int num_nodes)
{
  // Remove report handlers
  for (int i = 0; i < num_nodes; i++) {
    if (gtp_handle[i].u.handle != 0)
      rm_report_sm_xapp_api(gtp_handle[i].u.handle);
  }

  // Free allocated memory
  if (num_nodes > 0) {
    free(gtp_handle);
  }

  // Destroy hashtable
  if (ue_gtp_stats_by_rnti_ht)
    hashtable_destroy(&ue_gtp_stats_by_rnti_ht);

  // Destroy mutex
  pthread_mutex_destroy(&gtp_stats_mutex);

  // Stop xApp
  while (try_stop_xapp_api() == false)
    usleep(1000);

  printf("\nApplication cleaned up and terminated gracefully\n");
}

// static void sm_cb_gtp(sm_ag_if_rd_t const* rd)
// {
//   assert(rd != NULL);
//   // assert(rd->type == INDICATION_MSG_AGENT_IF_ANS_V0);
//   assert(rd->ind.type == GTP_STATS_V0);

//   // int64_t now = time_now_us();

//   for (u_int32_t i = 0; i < rd->ind.gtp.msg.len; i++) {
//     pthread_mutex_lock(&gtp_stats_mutex);
//     // printf("GTP ind_msg latency = %ld μs\n", now - rd->ind.mac.msg.tstamp);
//     gtp_ngu_t_stats_t* ue_gtp_stats = malloc(sizeof(*ue_gtp_stats));
//     *ue_gtp_stats = rd->ind.gtp.msg.ngut[i];
//     hashtable_insert(ue_gtp_stats_by_rnti_ht, rd->ind.gtp.msg.ngut[i].rnti, ue_gtp_stats);
//     if (ue_gtp_stats_by_rnti_ht != NULL) {
//       char buffer[8191] = {0};
//       int64_t now = time_now_us();
//       sprintf(buffer,
//               "{\"timestamp\": %ju, \"rnti_gtp\": %04x,\"teid_gnb\": %04x,\"teid_upf\": %d,\"qfi\": %d,\"ue_has_mqr\": %s, "
//               "\"rrc_ue_id\": %d,\"ue_rnti_t\": %d,\"mqr_rsrp\": %ld,\"mqr_rsrq\": %.1f,\"mqr_sinr\": %.1f,"
//               "\"ho_ue_id\": %d, \"ho_complete\": %d, \"ho_source_du\": %d, \"ho_target_du\": %d",
//               now / 1000,
//               rd->ind.gtp.msg.ngut[i].rnti,
//               rd->ind.gtp.msg.ngut[i].teidgnb,
//               rd->ind.gtp.msg.ngut[i].teidupf,
//               rd->ind.gtp.msg.ngut[i].qfi,
//               (rd->ind.gtp.msg.ngut[i].ue_context_has_mqr == 1) ? "true" : "false",
//               rd->ind.gtp.msg.ngut[i].ue_context_rrc_ue_id,
//               rd->ind.gtp.msg.ngut[i].ue_context_rnti_t,
//               rd->ind.gtp.msg.ngut[i].ue_context_mqr_rsrp,
//               rd->ind.gtp.msg.ngut[i].ue_context_mqr_rsrq,
//               rd->ind.gtp.msg.ngut[i].ue_context_mqr_sinr,
//               rd->ind.gtp.msg.ho_info.ue_id,
//               (rd->ind.gtp.msg.ho_info.ho_complete) ? 1 : 0,
//               rd->ind.gtp.msg.ho_info.source_du,
//               rd->ind.gtp.msg.ho_info.target_du);
//       if (rd->ind.gtp.msg.ho_info.target_du != 0)
//         exit(0);
//       long mqr_rsrp = 0; // UE MQR (Multi-Connectivity Quality Requirement)
//       double mqr_sinr = NAN;
//       double mqr_rsrq = NAN; // MQR Reference Signal Received Quality
//       if (rd->ind.gtp.msg.ngut[i].ue_context_has_mqr) {
//         mqr_rsrp = rd->ind.gtp.msg.ngut[i].ue_context_mqr_rsrp;
//         mqr_rsrq = rd->ind.gtp.msg.ngut[i].ue_context_mqr_rsrq;
//         mqr_sinr = rd->ind.gtp.msg.ngut[i].ue_context_mqr_sinr;
//         sprintf(&buffer[strlen(buffer)], "\"mqr_rsrp\": %ld,\"mqr_rsrq\": %.1f,\"mqr_sinr\": %.1f", mqr_rsrp, mqr_rsrq,
//         mqr_sinr);
//       }
//       sprintf(&buffer[strlen(buffer)], "}\n");

//       // Print the contents of the buffer
//       printf("Measured GTP KPI:\n%s\n", buffer);
//     } else {
//       printf("GTP Hashtable is empty\n");
//     }
//     pthread_mutex_unlock(&gtp_stats_mutex);
//   }

//   cnt_gtp++;
// }

// static void print_indication_message(ric_indication_t const* ind)
// {
//   assert(ind != NULL);

//   printf("\n=== Received GTP Indication at xApp ===\n");
//   printf("RIC Request ID: %d\n", ind->ric_id.ric_req_id);
//   printf("RIC Instance ID: %d\n", ind->ric_id.ric_inst_id);
//   printf("RAN Function ID: %d\n", ind->ric_id.ran_func_id);
//   printf("Action ID: %ld\n", ind->action_id);

//   if (ind->msg.buf != NULL) {
//     printf("\nIndication Message (%zu bytes):\n", ind->msg.len);

//     // Hex dump with ASCII
//     for (size_t i = 0; i < ind->msg.len; i++) {
//       if (i % 16 == 0) {
//         printf("\n%04zX: ", i);
//       }
//       printf("%02X ", ind->msg.buf[i]);

//       // Print ASCII after every 16 bytes or at the end
//       if ((i + 1) % 16 == 0 || i == ind->msg.len - 1) {
//         // Pad with spaces if needed
//         for (size_t j = i % 16; j < 15; j++) {
//           printf("   ");
//         }
//         printf(" |");
//         // Print ASCII characters
//         size_t start = i - (i % 16);
//         size_t end = i;
//         for (size_t j = start; j <= end; j++) {
//           char c = ind->msg.buf[j];
//           printf("%c", (c >= 32 && c <= 126) ? c : '.');
//         }
//         printf("|");
//       }
//     }
//     printf("\n");
//   }
// }
static void sm_cb_gtp(sm_ag_if_rd_t const* rd)
{
  assert(rd != NULL);
  assert(rd->ind.type == GTP_STATS_V0);

  char buffer[8191] = {0};

  pthread_mutex_lock(&gtp_stats_mutex);

  // Print the raw indication message
  // print_indication_message(&rd->ind.gtp);

  // Your existing GTP stats processing
  // gtp_ind_data_t const* ind = &rd->gtp_stats;
  // gtp_ind_data_t const* ind = &rd->ind.gtp;

  int64_t now = time_now_us();

  printf("\n=== Decoded GTP Stats ===\n");

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

    printf("GTP INFO:\n%s\n", buffer);
    exit(0);
  }
  pthread_mutex_unlock(&gtp_stats_mutex);
}

int main(int argc, char* argv[])
{
  // Initialize flexRIC arguments from command line
  fr_args_t args = init_fr_args(argc, argv);

  // Initialize mutex attributes and mutex for GTP only
  pthread_mutexattr_t attr = {0};
  pthread_mutex_init(&gtp_stats_mutex, &attr);

  // Initialize hash table for GTP statistics tracking
  ue_gtp_stats_by_rnti_ht = hashtable_create(MAX_MOBILES_PER_GNB, NULL, free);

  // Init the xApp
  init_xapp_api(&args);
  sleep(1);

  // Get connected E2 nodes
  e2_node_arr_xapp_t nodes = e2_nodes_xapp_api();

  // Verify at least one node is connected
  assert(nodes.len > 0);
  printf("Connected E2 nodes = %d\n", nodes.len);

  // Define reporting interval for GTP
  const char* i_3 = "1_ms";
  sm_ans_xapp_t* gtp_handle = NULL;

  if (nodes.len > 0) {
    gtp_handle = calloc(nodes.len, sizeof(sm_ans_xapp_t));
    assert(gtp_handle != NULL);
  }

  // Setup GTP reporting for each node
  for (int i = 0; i < nodes.len; i++) {
    e2_node_connected_xapp_t* n = &nodes.n[i];

    for (size_t j = 0; j < n->len_rf; j++)
      printf("Registered node %d ran func id = %d \n ", i, n->rf[j].id);

    // Setup GTP for appropriate node types
    if (n->id.type == ngran_gNB || n->id.type == ngran_eNB || n->id.type == ngran_gNB_CU || n->id.type == ngran_gNB_CUUP) {
      gtp_handle[i] = report_sm_xapp_api(&nodes.n[i].id, 148, (void*)i_3, sm_cb_gtp);
      assert(gtp_handle[i].success == true);
    }
  }

  // Main loop
  while (keepRunning) {
    usleep(100000); // Sleep for 100ms
  }

  // Cleanup when loop exits
  cleanup_resources(gtp_handle, nodes.len);

  // Free the E2 nodes array
  free_e2_node_arr_xapp(&nodes);

  printf("Application terminated successfully\n");
  return 0;
}
