/*
 * Licensed to the OpenAirInterface (OAI) Software Alliance under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The OpenAirInterface Software Alliance licenses this file to You under
 * the OAI Public License, Version 1.1  (the "License"); you may not use this file
 * except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.openairinterface.org/?page_id=698
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *-------------------------------------------------------------------------------
 * For more information about the OpenAirInterface (OAI) Software Alliance:
 *      contact@openairinterface.org
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
#include <unistd.h>

#define TICK_INTERVAL 10
#define UPDATE_INTERVAL 1000

static uint64_t cnt_mac;
static pthread_mutex_t mac_stats_mutex;
hash_table_t *ue_mac_stats_by_rnti_ht;
int16_t ticker = 0;

// static bool keepRunning = true;
// void intHandler() {
//     keepRunning = false;
// }


// void write_mac_stats(char * buffer, mac_ue_stats_impl_t ue_mac_stats) {
//     sprintf(&buffer[strlen(buffer)], "\"cellid\": %lu,\"in_sync\": %b, \"rnti\": \"%04x\",\"dlBytes\": %lu,\"dlMcs\": %d,\"dlBler\": %f,\"ulBytes\": %lu,"
//                                      "\"ulMcs\": %d,\"ulBler\": %f,\"ri\": %d,\"pmi\": \"(%d,%d)\",\"phr\": %d,\"pcmax\": %d,",
//             ue_mac_stats.nr_cellid,ue_mac_stats.in_sync,  ue_mac_stats.rnti, ue_mac_stats.dl_aggr_tbs,ue_mac_stats.dl_mcs1,ue_mac_stats.dl_bler,ue_mac_stats.ul_aggr_tbs,
//             ue_mac_stats.ul_mcs1,ue_mac_stats.ul_bler,ue_mac_stats.pmi_cqi_ri,ue_mac_stats.pmi_cqi_X1,ue_mac_stats.pmi_cqi_X2,
//             ue_mac_stats.phr,ue_mac_stats.pcmax);
// }

void write_mac_stats(char * buffer, mac_ue_stats_impl_t ue_mac_stats) {
    printf("cellid=%lu in_sync=%s rnti=%04x dlBytes=%lu dlMcs=%d dlBler=%.6f ulBytes=%lu ulMcs=%d ulBler=%.6f ri=%d pmi=(%d,%d) phr=%d pcmax=%d\n",
           ue_mac_stats.nr_cellid,
           ue_mac_stats.in_sync ? "true" : "false",
           ue_mac_stats.rnti,
           ue_mac_stats.dl_aggr_tbs,
           ue_mac_stats.dl_mcs1,
           ue_mac_stats.dl_bler,
           ue_mac_stats.ul_aggr_tbs,
           ue_mac_stats.ul_mcs1,
           ue_mac_stats.ul_bler,
           ue_mac_stats.pmi_cqi_ri,
           ue_mac_stats.pmi_cqi_X1,
           ue_mac_stats.pmi_cqi_X2,
           ue_mac_stats.phr,
           ue_mac_stats.pcmax);
    
    sprintf(&buffer[strlen(buffer)], "\"cellid\": %lu,\"in_sync\": %b, \"rnti\": \"%04x\",\"dlBytes\": %lu,\"dlMcs\": %d,\"dlBler\": %f,\"ulBytes\": %lu,"
                                     "\"ulMcs\": %d,\"ulBler\": %f,\"ri\": %d,\"pmi\": \"(%d,%d)\",\"phr\": %d,\"pcmax\": %d,",
            ue_mac_stats.nr_cellid,ue_mac_stats.in_sync,  ue_mac_stats.rnti, ue_mac_stats.dl_aggr_tbs,ue_mac_stats.dl_mcs1,ue_mac_stats.dl_bler,ue_mac_stats.ul_aggr_tbs,
            ue_mac_stats.ul_mcs1,ue_mac_stats.ul_bler,ue_mac_stats.pmi_cqi_ri,ue_mac_stats.pmi_cqi_X1,ue_mac_stats.pmi_cqi_X2,
            ue_mac_stats.phr,ue_mac_stats.pcmax);
}

static
void sm_cb_mac(sm_ag_if_rd_t const* rd)
{
  assert(rd != NULL);
  assert(rd->type ==INDICATION_MSG_AGENT_IF_ANS_V0);
  assert(rd->ind.type == MAC_STATS_V0);
 
  // int64_t now = time_now_us();
  // if(cnt_mac % 1024 == 0)
  //   printf("MAC ind_msg latency = %ld μs\n", now - rd->ind.mac.msg.tstamp);
  // cnt_mac++;
    for (u_int32_t i = 0; i < rd->ind.mac.msg.len_ue_stats; i++) {
      pthread_mutex_lock(&mac_stats_mutex);
      mac_ue_stats_impl_t *ue_mac_stats = malloc(sizeof(*ue_mac_stats));
      *ue_mac_stats = rd->ind.mac.msg.ue_stats[i];
      hashtable_insert(ue_mac_stats_by_rnti_ht, rd->ind.mac.msg.ue_stats[i].rnti, ue_mac_stats);
      pthread_mutex_unlock(&mac_stats_mutex);
  }
}

static
uint64_t cnt_rlc;

static
void sm_cb_rlc(sm_ag_if_rd_t const* rd)
{
  assert(rd != NULL);
  assert(rd->type ==INDICATION_MSG_AGENT_IF_ANS_V0);

  assert(rd->ind.type == RLC_STATS_V0);

  int64_t now = time_now_us();

  if(cnt_rlc % 1024 == 0)
    printf("RLC ind_msg latency = %ld μs\n", now - rd->ind.rlc.msg.tstamp);
  cnt_rlc++;
}

static
uint64_t cnt_pdcp;

static
void sm_cb_pdcp(sm_ag_if_rd_t const* rd)
{
  assert(rd != NULL);
  assert(rd->type == INDICATION_MSG_AGENT_IF_ANS_V0);

  assert(rd->ind.type == PDCP_STATS_V0);

  int64_t now = time_now_us();

  if(cnt_pdcp % 1024 == 0)
    printf("PDCP ind_msg latency = %ld μs\n", now - rd->ind.pdcp.msg.tstamp);

  cnt_pdcp++;
}

static
uint64_t cnt_gtp;

static
void sm_cb_gtp(sm_ag_if_rd_t const* rd)
{
  assert(rd != NULL);
  assert(rd->type ==INDICATION_MSG_AGENT_IF_ANS_V0);
  assert(rd->ind.type == GTP_STATS_V0);

  // int64_t now = time_now_us();
  // if(cnt_gtp % 1024 == 0)
  //   printf("GTP ind_msg latency = %ld μs\n", now - rd->ind.gtp.msg.tstamp);

  // cnt_gtp++;

  ticker += TICK_INTERVAL;
  if(ticker >= UPDATE_INTERVAL) {
      ticker = 0;
      for (uint32_t ue_idx = 0; ue_idx < rd->ind.gtp.msg.len; ue_idx++) {
          // Get updated mac stats of UE by RNTI
          pthread_mutex_lock(&mac_stats_mutex);
          void *data = NULL;
          hashtable_rc_t ret = hashtable_get(ue_mac_stats_by_rnti_ht, rd->ind.gtp.msg.ngut[ue_idx].ue_context_rnti_t, &data);
          pthread_mutex_unlock(&mac_stats_mutex);

          if ( ret == HASH_TABLE_OK ) {
              int64_t now = time_now_us();
              mac_ue_stats_impl_t ue_mac_stats = *(mac_ue_stats_impl_t *)data;
              if(!ue_mac_stats.in_sync)
              {
                  printf("UE: %04x is out of sync, removing stats hashtable entry\n", ue_mac_stats.rnti);
                  hashtable_remove(ue_mac_stats_by_rnti_ht, ue_mac_stats.rnti);
                  continue;
              }
              char buffer[8191] = {0};
              sprintf(buffer, "{\"ue_id\": %u,\"timestamp\": %ju,", rd->ind.gtp.msg.ngut[ue_idx].ue_context_rrc_ue_id, now / 1000);

              write_mac_stats(buffer, ue_mac_stats);

              double rsrq = NAN;
              double sinr = NAN;
              long rsrp = ue_mac_stats.rsrp;
              uint8_t cqi = ue_mac_stats.cqi;
              float rssi = -1 * ue_mac_stats.raw_rssi;
              float pucch_snr = ue_mac_stats.pucch_snr;
              float pusch_snr = ue_mac_stats.pusch_snr;

              if (rd->ind.gtp.msg.ngut[ue_idx].ue_context_has_mqr) {
                  rsrp = rd->ind.gtp.msg.ngut[ue_idx].ue_context_mqr_rsrp;
                  rsrq = rd->ind.gtp.msg.ngut[ue_idx].ue_context_mqr_rsrq;
                  sinr = rd->ind.gtp.msg.ngut[ue_idx].ue_context_mqr_sinr;
              }

              if (!isnan(rsrq)) {
                  sprintf(&buffer[strlen(buffer)], "\"rsrq\": %.1f, ", rsrq);
              }

              if (!isnan(sinr)) {
                  sprintf(&buffer[strlen(buffer)], "\"sinr\": %.1f, ", sinr);
              }

              sprintf(&buffer[strlen(buffer)],"\"rsrp\": %ld,\"rssi\": %.1f,\"cqi\": %d,\"pucchSnr\": %.1f,\"puschSnr\": %.1f",
                      rsrp,rssi,cqi,pucch_snr,pusch_snr);

              sprintf(&buffer[strlen(buffer)], "}\n");

              printf("%s\n", buffer);
          }
      }
  }
}


int main(int argc, char *argv[])
{
  fr_args_t args = init_fr_args(argc, argv);

  // struct sigaction act;
  // act.sa_handler = intHandler;
  // sigaction(SIGINT, &act, NULL);
  
  //Init the xApp
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


  if(nodes.len > 0){
    mac_handle = calloc( nodes.len, sizeof(sm_ans_xapp_t) ); 
    assert(mac_handle  != NULL);
    rlc_handle = calloc( nodes.len, sizeof(sm_ans_xapp_t) ); 
    assert(rlc_handle  != NULL);
    pdcp_handle = calloc( nodes.len, sizeof(sm_ans_xapp_t) ); 
    assert(pdcp_handle  != NULL);
    gtp_handle = calloc( nodes.len, sizeof(sm_ans_xapp_t) ); 
    assert(gtp_handle  != NULL);
  }

  for (int i = 0; i < nodes.len; i++) {
    e2_node_connected_xapp_t* n = &nodes.n[i];
    for (size_t j = 0; j < n->len_rf; j++)
      printf("Registered node %d ran func id = %d \n ", i, n->rf[j].id);

    if(n->id.type == ngran_gNB || n->id.type == ngran_eNB){
      // MAC Control is not yet implemented in OAI RAN
      // mac_ctrl_req_data_t wr = {.hdr.dummy = 1, .msg.action = 42 };
      // sm_ans_xapp_t const a = control_sm_xapp_api(&nodes.n[i].id, 142, &wr);
      // assert(a.success == true);

      mac_handle[i] = report_sm_xapp_api(&nodes.n[i].id, 142, (void*)i_0, sm_cb_mac);
      assert(mac_handle[i].success == true);

      rlc_handle[i] = report_sm_xapp_api(&nodes.n[i].id, 143, (void*)i_1, sm_cb_rlc);
      assert(rlc_handle[i].success == true);

      pdcp_handle[i] = report_sm_xapp_api(&nodes.n[i].id, 144, (void*)i_2, sm_cb_pdcp);
      assert(pdcp_handle[i].success == true);

      gtp_handle[i] = report_sm_xapp_api(&nodes.n[i].id, 148, (void*)i_3, sm_cb_gtp);
      assert(gtp_handle[i].success == true);

    } else if(n->id.type ==  ngran_gNB_CU || n->id.type ==  ngran_gNB_CUUP){
      pdcp_handle[i] = report_sm_xapp_api(&nodes.n[i].id, 144, (void*)i_2, sm_cb_pdcp);
      assert(pdcp_handle[i].success == true);

      gtp_handle[i] = report_sm_xapp_api(&nodes.n[i].id, 148, (void*)i_3, sm_cb_gtp);
      assert(gtp_handle[i].success == true);

    } else if(n->id.type == ngran_gNB_DU){
      mac_handle[i] = report_sm_xapp_api(&nodes.n[i].id, 142, (void*)i_0, sm_cb_mac);
      assert(mac_handle[i].success == true);

      rlc_handle[i] = report_sm_xapp_api(&nodes.n[i].id, 143, (void*)i_1, sm_cb_rlc);
      assert(rlc_handle[i].success == true);
    }

  }

  sleep(100000000);
  // while (keepRunning) {
  // }


  for(int i = 0; i < nodes.len; ++i){
    // Remove the handle previously returned
    if(mac_handle[i].u.handle != 0 )
      rm_report_sm_xapp_api(mac_handle[i].u.handle);
    if(rlc_handle[i].u.handle != 0) 
      rm_report_sm_xapp_api(rlc_handle[i].u.handle);
    if(pdcp_handle[i].u.handle != 0)
      rm_report_sm_xapp_api(pdcp_handle[i].u.handle);
    if(gtp_handle[i].u.handle != 0)
      rm_report_sm_xapp_api(gtp_handle[i].u.handle);
  }

  if(nodes.len > 0){
    free(mac_handle);
    free(rlc_handle);
    free(pdcp_handle);
    free(gtp_handle);
  }

  //Stop the xApp
  while(try_stop_xapp_api() == false)
    usleep(1000);


  printf("Test xApp run SUCCESSFULLY\n");
}



