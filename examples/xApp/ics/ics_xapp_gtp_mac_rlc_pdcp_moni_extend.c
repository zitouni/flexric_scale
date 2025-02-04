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

#include "../../../src/xApp/e42_xapp_api.h"
#include "../../../src/util/alg_ds/alg/defer.h"
#include "../../../src/util/time_now_us.h"
#include "../../../external/common/utils/hashtable/hashtable.h"

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>

#include <math.h>
#include <netdb.h>
#include <sys/socket.h>

#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <inttypes.h> // For PRIx32 macro

#define TICK_INTERVAL 10
#define UPDATE_INTERVAL 1000
#define MAX_MOBILES_PER_GNB 40

// static uint64_t cnt_mac;
static pthread_mutex_t mac_stats_mutex;
static pthread_mutex_t gtp_stats_mutex;

hash_table_t* ue_mac_stats_by_rnti_ht;
hash_table_t* ue_gtp_stats_by_rnti_ht;
int16_t ticker = 0;

static uint64_t cnt_gtp;

static volatile sig_atomic_t keepRunning = 1;

void intHandler()
{
  keepRunning = false;
}

void write_mac_stats(char* buffer, mac_ue_stats_impl_t* ue_mac_stats)
{
  sprintf(
      &buffer[strlen(buffer)],
      "\"MAC:\",\"cellid\": %lu,\"in_sync\": %d, \"rnti\": \"%04x\",\"dlBytes\": %lu,\"dlMcs\": %d,\"dlBler\": %f,\"ulBytes\": %ld,"
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
  // printf("Buffer contents:\n%s\n", buffer);
}

static void sm_cb_gtp(sm_ag_if_rd_t const* rd)
{
  assert(rd != NULL);
  assert(rd->type == INDICATION_MSG_AGENT_IF_ANS_V0);
  assert(rd->ind.type == GTP_STATS_V0);

  // int64_t now = time_now_us();

  for (u_int32_t i = 0; i < rd->ind.gtp.msg.len; i++) {
    pthread_mutex_lock(&gtp_stats_mutex);
    // printf("GTP ind_msg latency = %ld μs\n", now - rd->ind.mac.msg.tstamp);
    gtp_ngu_t_stats_t* ue_gtp_stats = malloc(sizeof(*ue_gtp_stats));
    *ue_gtp_stats = rd->ind.gtp.msg.ngut[i];
    hashtable_insert(ue_gtp_stats_by_rnti_ht, rd->ind.gtp.msg.ngut[i].rnti, ue_gtp_stats);
    if (ue_gtp_stats_by_rnti_ht != NULL) {
      char buffer[8191] = {0};
      int64_t now = time_now_us();
      sprintf(buffer,
              "{\"timestamp\": %ju, \"rnti_gtp\": %04x,\"teid_gnb\": %04x,\"teid_upf\": %d,\"qfi\": %d,\"ue_has_mqr\": %s, "
              "\"rrc_ue_id\": %d,\"ue_rnti_t\": %d,\"mqr_rsrp\": %ld,\"mqr_rsrq\": %.1f,\"mqr_sinr\": %.1f",
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
              rd->ind.gtp.msg.ngut[i].ue_context_mqr_sinr);

      long mqr_rsrp = 0; // UE MQR (Multi-Connectivity Quality Requirement)
      double mqr_sinr = NAN;
      double mqr_rsrq = NAN; // MQR Reference Signal Received Quality
      if (rd->ind.gtp.msg.ngut[i].ue_context_has_mqr) {
        mqr_rsrp = rd->ind.gtp.msg.ngut[i].ue_context_mqr_rsrp;
        mqr_rsrq = rd->ind.gtp.msg.ngut[i].ue_context_mqr_rsrq;
        mqr_sinr = rd->ind.gtp.msg.ngut[i].ue_context_mqr_sinr;
        sprintf(&buffer[strlen(buffer)], "\"mqr_rsrp\": %ld,\"mqr_rsrq\": %.1f,\"mqr_sinr\": %.1f", mqr_rsrp, mqr_rsrq, mqr_sinr);
      }
      sprintf(&buffer[strlen(buffer)], "}\n");

      // Print the contents of the buffer
      // printf("Measured GTP KPI:\n%s\n", buffer);
    } else {
      printf("GTP Hashtable is empty\n");
    }
    pthread_mutex_unlock(&gtp_stats_mutex);
  }

  cnt_gtp++;
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
      // printf("Measured MAC KPI:\n%s\n", buffer);
    } else {
      printf("Hash is empty\n");
      // Hashtable is empty, insertion might have failed
    }
    pthread_mutex_unlock(&mac_stats_mutex);
  }
}

static uint64_t cnt_rlc;

static void sm_cb_rlc(sm_ag_if_rd_t const* rd)
{
  assert(rd != NULL);
  assert(rd->type == INDICATION_MSG_AGENT_IF_ANS_V0);
  assert(rd->ind.type == RLC_STATS_V0);

  int64_t now = time_now_us();

  if (cnt_rlc % 1024 == 0)
    printf("RLC ind_msg latency = %ld μs\n", now - rd->ind.rlc.msg.tstamp);
  cnt_rlc++;
}

static uint64_t cnt_pdcp;

static void sm_cb_pdcp(sm_ag_if_rd_t const* rd)
{
  assert(rd != NULL);
  assert(rd->type == INDICATION_MSG_AGENT_IF_ANS_V0);

  assert(rd->ind.type == PDCP_STATS_V0);

  int64_t now = time_now_us();

  if (cnt_pdcp % 1024 == 0)
    printf("PDCP ind_msg latency = %ld μs\n", now - rd->ind.pdcp.msg.tstamp);

  cnt_pdcp++;
}

void setup_signal_handler()
{
  struct sigaction act;
  memset(&act, 0, sizeof(act));
  act.sa_handler = intHandler;
  sigaction(SIGINT, &act, NULL);
  sigaction(SIGTERM, &act, NULL);
}

void cleanup_resources(sm_ans_xapp_t* mac_handle,
                       sm_ans_xapp_t* rlc_handle,
                       sm_ans_xapp_t* pdcp_handle,
                       sm_ans_xapp_t* gtp_handle,
                       int num_nodes)
{
  // Remove report handlers
  for (int i = 0; i < num_nodes; i++) {
    if (mac_handle[i].u.handle != 0)
      rm_report_sm_xapp_api(mac_handle[i].u.handle);
    if (gtp_handle[i].u.handle != 0)
      rm_report_sm_xapp_api(gtp_handle[i].u.handle);
    if (rlc_handle[i].u.handle != 0)
      rm_report_sm_xapp_api(rlc_handle[i].u.handle);
    if (pdcp_handle[i].u.handle != 0)
      rm_report_sm_xapp_api(pdcp_handle[i].u.handle);
  }

  // Free allocated memory
  if (num_nodes > 0) {
    free(mac_handle);
    free(rlc_handle);
    free(pdcp_handle);
    free(gtp_handle);
  }

  // Destroy hashtables
  if (ue_mac_stats_by_rnti_ht)
    hashtable_destroy(&ue_mac_stats_by_rnti_ht);
  if (ue_gtp_stats_by_rnti_ht)
    hashtable_destroy(&ue_gtp_stats_by_rnti_ht);

  // Destroy mutexes
  pthread_mutex_destroy(&mac_stats_mutex);
  pthread_mutex_destroy(&gtp_stats_mutex);

  // Stop xApp
  while (try_stop_xapp_api() == false)
    usleep(1000);

  printf("\nApplication cleaned up and terminated gracefully\n");
}

int main(int argc, char* argv[])
{
  fr_args_t args = init_fr_args(argc, argv);

  // struct sigaction act;
  // act.sa_handler = intHandler;
  // sigaction(SIGINT, &act, NULL);
  pthread_mutexattr_t attr = {0};
  pthread_mutex_init(&mac_stats_mutex, &attr);
  pthread_mutex_init(&gtp_stats_mutex, &attr);

  ue_mac_stats_by_rnti_ht = hashtable_create(MAX_MOBILES_PER_GNB, NULL, free);
  ue_gtp_stats_by_rnti_ht = hashtable_create(MAX_MOBILES_PER_GNB, NULL, free);
  // Init the xApp
  init_xapp_api(&args);
  sleep(1);

  e2_node_arr_xapp_t nodes = e2_nodes_xapp_api();
  defer({ free_e2_node_arr_xapp(&nodes); });

  assert(nodes.len > 0);

  printf("Connected E2 nodes = %d\n", nodes.len);

  // MAC indication
  const char* i_0 = "1_ms";
  sm_ans_xapp_t* mac_handle = NULL;
  // RLC indication
  const char* i_1 = "1_ms";
  sm_ans_xapp_t* rlc_handle = NULL;
  // PDCP indication
  const char* i_2 = "1_ms";
  sm_ans_xapp_t* pdcp_handle = NULL;
  // GTP indication
  const char* i_3 = "1_ms";
  sm_ans_xapp_t* gtp_handle = NULL;

  if (nodes.len > 0) {
    mac_handle = calloc(nodes.len, sizeof(sm_ans_xapp_t));
    assert(mac_handle != NULL);
    rlc_handle = calloc(nodes.len, sizeof(sm_ans_xapp_t));
    assert(rlc_handle != NULL);
    pdcp_handle = calloc(nodes.len, sizeof(sm_ans_xapp_t));
    assert(pdcp_handle != NULL);
    gtp_handle = calloc(nodes.len, sizeof(sm_ans_xapp_t));
    assert(gtp_handle != NULL);
  }

  for (int i = 0; i < nodes.len; i++) {
    e2_node_connected_xapp_t* n = &nodes.n[i];
    for (size_t j = 0; j < n->len_rf; j++)
      printf("Registered node %d ran func id = %d \n ", i, n->rf[j].id);

    if (n->id.type == ngran_gNB || n->id.type == ngran_eNB) {
      mac_handle[i] = report_sm_xapp_api(&nodes.n[i].id, 142, (void*)i_0, sm_cb_mac);
      assert(mac_handle[i].success == true);

      // rlc_handle[i] = report_sm_xapp_api(&nodes.n[i].id, 143, (void*)i_1, sm_cb_rlc);
      // assert(rlc_handle[i].success == true);

      gtp_handle[i] = report_sm_xapp_api(&nodes.n[i].id, 148, (void*)i_3, sm_cb_gtp);
      assert(gtp_handle[i].success == true);

      // pdcp_handle[i] = report_sm_xapp_api(&nodes.n[i].id, 144, (void*)i_2, sm_cb_pdcp);
      // assert(pdcp_handle[i].success == true);

    } else if (n->id.type == ngran_gNB_CU || n->id.type == ngran_gNB_CUUP) {
      pdcp_handle[i] = report_sm_xapp_api(&nodes.n[i].id, 144, (void*)i_2, sm_cb_pdcp);
      assert(pdcp_handle[i].success == true);

      gtp_handle[i] = report_sm_xapp_api(&nodes.n[i].id, 148, (void*)i_3, sm_cb_gtp);
      assert(gtp_handle[i].success == true);

    } else if (n->id.type == ngran_gNB_DU) {
      mac_handle[i] = report_sm_xapp_api(&nodes.n[i].id, 142, (void*)i_0, sm_cb_mac);
      assert(mac_handle[i].success == true);

      rlc_handle[i] = report_sm_xapp_api(&nodes.n[i].id, 143, (void*)i_1, sm_cb_rlc);
      assert(rlc_handle[i].success == true);
    }
  }

  // sleep(100000000);
  while (keepRunning) {
    usleep(100000); // Sleep for 100ms or adjust as needed
  }

  // Cleanup when loop exits
  cleanup_resources(mac_handle, rlc_handle, pdcp_handle, gtp_handle, nodes.len);
}
