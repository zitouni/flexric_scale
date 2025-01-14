/*
 * File: latency_xapp.c
 * Author: Rafik ZITOUNI
 * Date: 2024-12-11
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
#include "../../../src/util/alg_ds/ds/lock_guard/lock_guard.h"
#include "../../../src/util/e.h"

#include "db_metrics.h"
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>

// Constants
#define INIT_WAIT_TIME_S 2
#define RETRY_INTERVAL_US (100000) /* 100ms */
#define MAX_RETRY_ATTEMPTS (3) /* Maximum number of retry attempts */
#define SHUTDOWN_WAIT_TIME_S (1) /* Wait time during shutdown */

// Structure to maintain xApp state
typedef struct {
  sm_ans_xapp_t* handles; // Array of handles for service model answers
  size_t node_count; // Number of E2 nodes
  pthread_mutex_t mutex; // Mutex for thread safety
  bool initialized; // Flag to track initialization status
} xapp_state_t;

// static uint64_t const period_ms = 1000;
// static uint64_t const period_ms = 1000;
static uint64_t const period_ms = 100;
// static uint64_t const period_ms = 5000;
static pthread_mutex_t mtx;

static bool keepRunning = true;
static void intHandler()
{
  printf("\nReceived shutdown signal. Cleaning up...\n");
  // Set the flag first
  keepRunning = false;

  // Flush any pending output
  fflush(stdout);
  fflush(stderr);
}

static void log_gnb_ue_id(ue_id_e2sm_t ue_id)
{
  if (ue_id.gnb.gnb_cu_ue_f1ap_lst != NULL) {
    for (size_t i = 0; i < ue_id.gnb.gnb_cu_ue_f1ap_lst_len; i++) {
      printf("\nUE ID type = gNB-CU, gnb_cu_ue_f1ap = %u\n", ue_id.gnb.gnb_cu_ue_f1ap_lst[i]);
    }
  } else {
    printf("UE ID type = gNB, amf_ue_ngap_id = %lu\n", ue_id.gnb.amf_ue_ngap_id);
  }
  if (ue_id.gnb.ran_ue_id != NULL) {
    printf("ran_ue_id = %lx\n", *ue_id.gnb.ran_ue_id); // RAN UE NGAP ID
  }
}

static void log_du_ue_id(ue_id_e2sm_t ue_id)
{
  printf("\nUE ID type = gNB-DU, gnb_cu_ue_f1ap = %u\n", ue_id.gnb_du.gnb_cu_ue_f1ap);
  if (ue_id.gnb_du.ran_ue_id != NULL) {
    printf("ran_ue_id = %lx\n", *ue_id.gnb_du.ran_ue_id); // RAN UE NGAP ID
  }
}

static void log_cuup_ue_id(ue_id_e2sm_t ue_id)
{
  printf("UE ID type = gNB-CU-UP, gnb_cu_cp_ue_e1ap = %u\n", ue_id.gnb_cu_up.gnb_cu_cp_ue_e1ap);
  if (ue_id.gnb_cu_up.ran_ue_id != NULL) {
    printf("ran_ue_id = %lx\n", *ue_id.gnb_cu_up.ran_ue_id); // RAN UE NGAP ID
  }
}

typedef void (*log_ue_id)(ue_id_e2sm_t ue_id);

static log_ue_id log_ue_id_e2sm[END_UE_ID_E2SM] = {
    log_gnb_ue_id, // common for gNB-mono, CU and CU-CP
    log_du_ue_id,
    log_cuup_ue_id,
    NULL,
    NULL,
    NULL,
    NULL,
};

static void log_int_value(byte_array_t name, meas_record_lst_t meas_record)
{
  if (cmp_str_ba("RRU.PrbTotDl", name) == 0) {
    printf("RRU.PrbTotDl = %d [PRBs]\n", meas_record.int_val);
  } else if (cmp_str_ba("RRU.PrbTotUl", name) == 0) {
    printf("RRU.PrbTotUl = %d [PRBs]\n", meas_record.int_val);
  } else if (cmp_str_ba("DRB.PdcpSduVolumeDL", name) == 0) {
    printf("DRB.PdcpSduVolumeDL = %d [kb]\n", meas_record.int_val);
  } else if (cmp_str_ba("DRB.PdcpSduVolumeUL", name) == 0) {
    printf("DRB.PdcpSduVolumeUL = %d [kb]\n", meas_record.int_val);
  } else {
    printf("Measurement Name not yet supported\n");
  }
}

static void log_real_value(byte_array_t name, meas_record_lst_t meas_record)
{
  if (cmp_str_ba("DRB.RlcSduDelayDl", name) == 0) {
    printf("DRB.RlcSduDelayDl = %.2f [μs]\n", meas_record.real_val);
  } else if (cmp_str_ba("DRB.UEThpDl", name) == 0) {
    printf("DRB.UEThpDl = %.2f [kbps]\n", meas_record.real_val);
  } else if (cmp_str_ba("DRB.UEThpUl", name) == 0) {
    printf("DRB.UEThpUl = %.2f [kbps]\n", meas_record.real_val);
  } else {
    printf("Measurement Name not yet supported\n");
  }
}

typedef void (*log_meas_value)(byte_array_t name, meas_record_lst_t meas_record);

static log_meas_value get_meas_value[END_MEAS_VALUE] = {
    log_int_value,
    log_real_value,
    NULL,
};

static void match_meas_name_type(meas_type_t meas_type, meas_record_lst_t meas_record)
{
  // Get the value of the Measurement
  get_meas_value[meas_record.value](meas_type.name, meas_record);
}

static void match_id_meas_type(meas_type_t meas_type, meas_record_lst_t meas_record)
{
  (void)meas_type;
  (void)meas_record;
  assert(false && "ID Measurement Type not yet supported");
}

typedef void (*check_meas_type)(meas_type_t meas_type, meas_record_lst_t meas_record);

static check_meas_type match_meas_type[END_MEAS_TYPE] = {
    match_meas_name_type,
    match_id_meas_type,
};

static void log_kpm_measurements(kpm_ind_msg_format_1_t const* msg_frm_1)
{
  assert(msg_frm_1->meas_info_lst_len > 0 && "Cannot correctly print measurements");

  // UE Measurements per granularity period
  for (size_t j = 0; j < msg_frm_1->meas_data_lst_len; j++) {
    meas_data_lst_t const data_item = msg_frm_1->meas_data_lst[j];

    for (size_t z = 0; z < data_item.meas_record_len; z++) {
      meas_type_t const meas_type = msg_frm_1->meas_info_lst[z].meas_type;
      meas_record_lst_t const record_item = data_item.meas_record_lst[z];

      match_meas_type[meas_type.type](meas_type, record_item);

      if (data_item.incomplete_flag && *data_item.incomplete_flag == TRUE_ENUM_VALUE)
        printf("Measurement Record not reliable");
    }
  }
}

// Function to print node metrics with counter and both timestamps
static void print_node_metrics(e2_node_arr_xapp_t const* nodes, int counter, int64_t latency, int64_t timestamp, char* time_str)
{
  assert(nodes != NULL);

  if (nodes->len == 0) {
    printf("\nWarning: No E2 nodes information available\n");
    return;
  }

  for (size_t i = 0; i < nodes->len; ++i) {
    printf("\n #%d [%ld | %s] Latency: %ld μs", counter, timestamp, time_str, latency);
  }
}

// Rest of the code remains the same
static void sm_cb_kpm(sm_ag_if_rd_t const* rd)
{
  assert(rd != NULL);
  assert(rd->type == INDICATION_MSG_AGENT_IF_ANS_V0);
  assert(rd->ind.type == KPM_STATS_V3_0);

  // Capture timestamp immediately
  int64_t const now = time_now_us();

  // Reading Indication Message Format 3
  kpm_ind_data_t const* ind = &rd->ind.kpm.ind;
  kpm_ric_ind_hdr_format_1_t const* hdr_frm_1 = &ind->hdr.kpm_ric_ind_hdr_format_1;
  kpm_ind_msg_format_3_t const* msg_frm_3 = &ind->msg.frm_3;

  // Calculate latency before taking lock
  int64_t latency = now - hdr_frm_1->collectStartTime;

  // Get human readable time
  time_t now_readable = now / 1000000; // Convert microseconds to seconds
  struct tm* tm_info = localtime(&now_readable);
  char time_str[26];
  strftime(time_str, 26, "%Y-%m-%d %H:%M:%S", tm_info);

  static int counter = 1;
  {
    lock_guard(&mtx);

    // Get and print E2 Node information in one line
    e2_node_arr_xapp_t nodes = e2_nodes_xapp_api();
    print_node_metrics(&nodes, counter, latency, now, time_str);
    // Store detailed metrics in database
    store_detailed_metrics(&rd->ind.kpm.ind, latency, counter, &nodes, now, time_str);

    // Reported list of measurements per UE
    for (size_t i = 0; i < msg_frm_3->ue_meas_report_lst_len; i++) {
      // log UE ID
      ue_id_e2sm_t const ue_id_e2sm = msg_frm_3->meas_report_per_ue[i].ue_meas_report_lst;
      ue_id_e2sm_e const type = ue_id_e2sm.type;
      log_ue_id_e2sm[type](ue_id_e2sm);

      // log measurements
      log_kpm_measurements(&msg_frm_3->meas_report_per_ue[i].ind_msg_format_1);
    }
    counter++;
  }
}

static test_info_lst_t filter_predicate(test_cond_type_e type, test_cond_e cond, int value)
{
  test_info_lst_t dst = {0};

  dst.test_cond_type = type;
  // It can only be TRUE_TEST_COND_TYPE so it does not matter the type
  // but ugly ugly...
  dst.S_NSSAI = TRUE_TEST_COND_TYPE;

  dst.test_cond = calloc(1, sizeof(test_cond_e));
  assert(dst.test_cond != NULL && "Memory exhausted");
  *dst.test_cond = cond;

  dst.test_cond_value = calloc(1, sizeof(test_cond_value_t));
  assert(dst.test_cond_value != NULL && "Memory exhausted");
  dst.test_cond_value->type = OCTET_STRING_TEST_COND_VALUE;

  dst.test_cond_value->octet_string_value = calloc(1, sizeof(byte_array_t));
  assert(dst.test_cond_value->octet_string_value != NULL && "Memory exhausted");
  const size_t len_nssai = 1;
  dst.test_cond_value->octet_string_value->len = len_nssai;
  dst.test_cond_value->octet_string_value->buf = calloc(len_nssai, sizeof(uint8_t));
  assert(dst.test_cond_value->octet_string_value->buf != NULL && "Memory exhausted");
  dst.test_cond_value->octet_string_value->buf[0] = value;

  return dst;
}

static label_info_lst_t fill_kpm_label(void)
{
  label_info_lst_t label_item = {0};

  label_item.noLabel = ecalloc(1, sizeof(enum_value_e));
  *label_item.noLabel = TRUE_ENUM_VALUE;

  return label_item;
}

static kpm_act_def_format_1_t fill_act_def_frm_1(ric_report_style_item_t const* report_item)
{
  assert(report_item != NULL);
  // assert(report_item->act_def_format_type == FORMAT_1_ACTION_DEFINITION);

  kpm_act_def_format_1_t ad_frm_1 = {0};

  size_t const sz = report_item->meas_info_for_action_lst_len;

  // [1, 65535]
  ad_frm_1.meas_info_lst_len = sz;
  ad_frm_1.meas_info_lst = calloc(sz, sizeof(meas_info_format_1_lst_t));
  assert(ad_frm_1.meas_info_lst != NULL && "Memory exhausted");

  for (size_t i = 0; i < sz; i++) {
    meas_info_format_1_lst_t* meas_item = &ad_frm_1.meas_info_lst[i];
    // 8.3.9
    // Measurement Name
    meas_item->meas_type.type = NAME_MEAS_TYPE;
    meas_item->meas_type.name = copy_byte_array(report_item->meas_info_for_action_lst[i].name);

    // [1, 2147483647]
    // 8.3.11
    meas_item->label_info_lst_len = 1;
    meas_item->label_info_lst = ecalloc(1, sizeof(label_info_lst_t));
    meas_item->label_info_lst[0] = fill_kpm_label();
  }

  // 8.3.8 [0, 4294967295]
  ad_frm_1.gran_period_ms = period_ms;

  // 8.3.20 - OPTIONAL
  ad_frm_1.cell_global_id = NULL;

#if defined KPM_V2_03 || defined KPM_V3_00
  // [0, 65535]
  ad_frm_1.meas_bin_range_info_lst_len = 0;
  ad_frm_1.meas_bin_info_lst = NULL;
#endif

  return ad_frm_1;
}

static kpm_act_def_t fill_report_style_4(ric_report_style_item_t const* report_item)
{
  assert(report_item != NULL);
  assert((report_item->act_def_format_type == FORMAT_4_ACTION_DEFINITION)
         || (report_item->act_def_format_type == FORMAT_1_ACTION_DEFINITION));

  kpm_act_def_t act_def = {.type = FORMAT_4_ACTION_DEFINITION};
  // kpm_act_def_t act_def; // = {.type = FORMAT_4_ACTION_DEFINITION};
  // if (report_item->act_def_format_type == FORMAT_4_ACTION_DEFINITION){
  //   act_def = (kpm_act_def_t){ .type = FORMAT_4_ACTION_DEFINITION };
  // }else{
  //   if(report_item->act_def_format_type == FORMAT_1_ACTION_DEFINITION){
  //     act_def = (kpm_act_def_t){ .type = FORMAT_1_ACTION_DEFINITION };
  //   }
  // }
  // Fill matching condition
  // [1, 32768]
  act_def.frm_4.matching_cond_lst_len = 1;
  act_def.frm_4.matching_cond_lst = calloc(act_def.frm_4.matching_cond_lst_len, sizeof(matching_condition_format_4_lst_t));
  assert(act_def.frm_4.matching_cond_lst != NULL && "Memory exhausted");
  // Filter connected UEs by S-NSSAI criteria
  test_cond_type_e const type = S_NSSAI_TEST_COND_TYPE; // CQI_TEST_COND_TYPE
  test_cond_e const condition = EQUAL_TEST_COND; // GREATERTHAN_TEST_COND
  int const value = 1;
  act_def.frm_4.matching_cond_lst[0].test_info_lst = filter_predicate(type, condition, value);

  // Fill Action Definition Format 1
  // 8.2.1.2.1
  // act_def.frm_1 = fill_act_def_frm_1(report_item);
  act_def.frm_4.action_def_format_1 = fill_act_def_frm_1(report_item);

  return act_def;
}

typedef kpm_act_def_t (*fill_kpm_act_def)(ric_report_style_item_t const* report_item);

static fill_kpm_act_def get_kpm_act_def[END_RIC_SERVICE_REPORT] = {
    fill_report_style_4, // fill_act_def_frm_1, //fill_report_style_1,
    NULL,
    NULL,
    fill_report_style_4,
    NULL,
};

static kpm_sub_data_t gen_kpm_subs(kpm_ran_function_def_t const* ran_func)
{
  assert(ran_func != NULL);
  assert(ran_func->ric_event_trigger_style_list != NULL);

  kpm_sub_data_t kpm_sub = {0};

  // Generate Event Trigger
  assert(ran_func->ric_event_trigger_style_list[0].format_type == FORMAT_1_RIC_EVENT_TRIGGER);
  kpm_sub.ev_trg_def.type = FORMAT_1_RIC_EVENT_TRIGGER;
  kpm_sub.ev_trg_def.kpm_ric_event_trigger_format_1.report_period_ms = period_ms;

  // Generate Action Definition
  kpm_sub.sz_ad = 1;
  kpm_sub.ad = calloc(kpm_sub.sz_ad, sizeof(kpm_act_def_t));
  assert(kpm_sub.ad != NULL && "Memory exhausted");

  // Multiple Action Definitions in one SUBSCRIPTION message is not supported in this project
  // Multiple REPORT Styles = Multiple Action Definition = Multiple SUBSCRIPTION messages
  assert(ran_func->ric_report_style_list != NULL);
  ric_report_style_item_t* const report_item = &ran_func->ric_report_style_list[0];

  ric_service_report_e const report_style_type = report_item->report_style_type;
  //*kpm_sub.ad = get_kpm_act_def[report_style_type](report_item);
  if (kpm_sub.ad != NULL) {
    if (report_style_type >= 0 && report_style_type < END_RIC_SERVICE_REPORT) {
      if (get_kpm_act_def[report_style_type] != NULL && report_item != NULL) {
        *kpm_sub.ad = get_kpm_act_def[report_style_type](report_item);
      } else {
        // Handle the case where the function pointer or report_item is NULL
        printf("Error: Function pointer or report_item is NULL\n");
      }
    } else {
      // Handle the case where report_style_type is out of bounds
      printf("Error: report_style_type is out of bounds\n");
    }
  } else {
    // Handle the case where kpm_sub.ad is NULL
    printf("Error: kpm_sub.ad is NULL\n");
  }

  return kpm_sub;
}

static bool eq_sm(sm_ran_function_t const* elem, int const id)
{
  if (elem->id == id)
    return true;

  return false;
}

static size_t find_sm_idx(sm_ran_function_t* rf, size_t sz, bool (*f)(sm_ran_function_t const*, int const), int const id)
{
  for (size_t i = 0; i < sz; i++) {
    if (f(&rf[i], id))
      return i;
  }

  assert(0 != 0 && "SM ID could not be found in the RAN Function List");
}

/**
 * Initialize xApp state structure
 * @param node_count Number of E2 nodes to handle
 * @return Pointer to initialized state or NULL on failure
 */
static xapp_state_t* init_xapp_state(size_t node_count)
{
  // Allocate main state structure
  xapp_state_t* state = calloc(1, sizeof(xapp_state_t));
  if (!state) {
    fprintf(stderr, "Failed to allocate xApp state\n");
    return NULL;
  }

  // Allocate handles array
  state->handles = calloc(node_count, sizeof(sm_ans_xapp_t));
  if (!state->handles) {
    fprintf(stderr, "Failed to allocate handles\n");
    free(state);
    return NULL;
  }

  // Initialize mutex for thread safety
  if (pthread_mutex_init(&state->mutex, NULL) != 0) {
    fprintf(stderr, "Failed to initialize mutex\n");
    free(state->handles);
    free(state);
    return NULL;
  }

  state->node_count = node_count;
  state->initialized = true;
  return state;
}

/**
 * Clean up xApp state and release resources
 * @param state Pointer to xApp state structure
 */
static void cleanup_xapp_state(xapp_state_t* state)
{
  if (!state)
    return;

  if (state->initialized) {
    pthread_mutex_destroy(&state->mutex);
    free(state->handles);
  }
  free(state);
}

/**
 * Set up KPM subscription for a node
 * @param node E2 node to configure
 * @param handle Handle to store subscription result
 * @param kpm_function KPM function identifier
 * @return true if subscription successful, false otherwise
 */
static bool setup_kpm_subscription(e2_node_connected_xapp_t* node, sm_ans_xapp_t* handle, int kpm_function)
{
  // Find service model index for KPM function
  size_t idx = find_sm_idx(node->rf, node->len_rf, eq_sm, kpm_function);
  if (idx == (size_t)-1) {
    fprintf(stderr, "KPM function not found\n");
    return false;
  }

  // Validate KPM function type
  if (node->rf[idx].defn.type != KPM_RAN_FUNC_DEF_E) {
    fprintf(stderr, "Invalid KPM RAN Function type\n");
    return false;
  }

  // Check if report style list is available
  if (!node->rf[idx].defn.kpm.ric_report_style_list) {
    return false;
  }

  // Generate and setup KPM subscription
  kpm_sub_data_t kpm_sub = gen_kpm_subs(&node->rf[idx].defn.kpm);
  *handle = report_sm_xapp_api(&node->id, kpm_function, &kpm_sub, sm_cb_kpm);
  free_kpm_sub_data(&kpm_sub);

  return handle->success;
}

// Add helper function for xApp stop
static bool wait_for_xapp_stop(void)
{
  for (int i = 0; i < MAX_RETRY_ATTEMPTS; i++) {
    if (try_stop_xapp_api()) {
      return true;
    }
    usleep(RETRY_INTERVAL_US);
  }
  return false;
}

static void cleanup_subscriptions(e2_node_arr_xapp_t* nodes, sm_ans_xapp_t* handles, size_t num_nodes)
{
  if (nodes == NULL || handles == NULL) {
    return;
  }

  for (size_t i = 0; i < num_nodes; ++i) {
    if (handles[i].success) { // Check if subscription was successful
      printf("Removing subscription for node %zu...\n", i);
      rm_report_sm_xapp_api(handles[i].u.handle); // Use the correct API function
      // Add small delay between unsubscribe operations
      usleep(100000); // 100ms delay
    }
  }
}

static void cleanup_resources(xapp_state_t* state, e2_node_arr_xapp_t* nodes, bool nodes_initialized, int* ret_code)
{
  // Cleanup KPM subscriptions
  if (state) {
    for (size_t i = 0; i < nodes->len; ++i) {
      if (state->handles[i].success) {
        rm_report_sm_xapp_api(state->handles[i].u.handle);
      }
    }
  }

  // Attempt graceful xApp shutdown
  if (!wait_for_xapp_stop()) {
    fprintf(stderr, "Failed to stop xApp gracefully\n");
    *ret_code = -1;
  }

  // Release all resources
  if (nodes_initialized) {
    free_e2_node_arr_xapp(nodes);
  }
  cleanup_xapp_state(state);

  if (*ret_code == 0) {
    printf("Test xApp run SUCCESSFULLY\n");
  }
  // Before exit, close database
  close_metrics_db();
}

int main(int argc, char* argv[])
{
  int ret_code = -1;
  xapp_state_t* state = NULL;
  e2_node_arr_xapp_t nodes = {0};
  bool nodes_initialized = false;

  // Initialize command line arguments
  fr_args_t args = init_fr_args(argc, argv);

  // Setup signal handler for graceful shutdown
  struct sigaction act = {.sa_handler = intHandler, .sa_flags = 0};
  sigemptyset(&act.sa_mask);
  if (sigaction(SIGINT, &act, NULL) != 0) {
    fprintf(stderr, "Failed to setup signal handler\n");
    cleanup_resources(state, &nodes, nodes_initialized, &ret_code);
    return ret_code;
  }

  // Initialize xApp API
  init_xapp_api(&args);

  // Initialize database
  if (!init_metrics_db("latency_kpm_metrics.db")) {
    fprintf(stderr, "Failed to initialize metrics database\n");
    // Handle error or continue without database
  }

  // Wait for initialization to complete
  sleep(INIT_WAIT_TIME_S);

  // Get connected E2 nodes
  nodes = e2_nodes_xapp_api();
  nodes_initialized = true;

  if (nodes.len == 0) {
    fprintf(stderr, "No E2 nodes available\n");
    cleanup_resources(state, &nodes, nodes_initialized, &ret_code);
    return ret_code;
  }

  printf("Connected E2 nodes = %d\n", nodes.len);

  // Initialize xApp state
  state = init_xapp_state(nodes.len);
  if (!state) {
    cleanup_resources(state, &nodes, nodes_initialized, &ret_code);
    return ret_code;
  }

  // Setup KPM subscriptions for each node
  const int KPM_ran_function = 2;
  for (size_t i = 0; i < nodes.len; ++i) {
    if (!setup_kpm_subscription(&nodes.n[i], &state->handles[i], KPM_ran_function)) {
      fprintf(stderr, "Failed to setup KPM subscription for node %zu\n", i);
      // Continue with other nodes
    }
  }

  // Main processing loop with timeout
  //   time_t start_time = time(NULL);
  //   while (keepRunning && (time(NULL) - start_time) < MAIN_LOOP_TIMEOUT_S) {
  //     // Process events here
  //     usleep(RETRY_INTERVAL_US);
  //   }

  // Main processing loop - run indefinitely until signal received
  while (keepRunning) {
    // Process events here
    usleep(RETRY_INTERVAL_US);
  }

  // Graceful shutdown sequence
  printf("Starting graceful shutdown...\n");

  // Clean up subscriptions first
  if (state != NULL && state->handles != NULL) {
    cleanup_subscriptions(&nodes, state->handles, nodes.len);
  }

  // Small delay to allow pending operations to complete
  sleep(1);
  // Mark successful execution
  ret_code = 0;

  cleanup_resources(state, &nodes, nodes_initialized, &ret_code);
  return ret_code;
}
