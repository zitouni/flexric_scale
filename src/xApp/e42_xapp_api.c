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

#include "e42_xapp_api.h"
#include "e42_xapp.h"
#include "../util/conf_file.h"
#include "../lib/e2ap/e2ap_global_node_id_wrapper.h"
#include "../util/alg_ds/alg/defer.h"
#include "../util/alg_ds/alg/alg.h"
#include "../sm/slice_sm/slice_sm_id.h"
#include "../sm/tc_sm/tc_sm_id.h"
#include "../sm/rc_sm/rc_sm_id.h"
#include "../sm/mac_sm/mac_sm_id.h"

#include <signal.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

static e42_xapp_t* xapp = NULL;

static pthread_t thrd_xapp;

static void sig_handler(int sig_num)
{
  printf("\n[xApp]: Initiating graceful shutdown with signal number = %d\n", sig_num);

  const int MAX_RETRIES = 5;
  const useconds_t RETRY_DELAY = 1000; // 1ms delay as before
  int retry_count = 0;
  bool stopped = false;

  // First ensure all pending messages are processed
  while (not_dispatch_msg(xapp) > 0 && retry_count < MAX_RETRIES) {
    usleep(RETRY_DELAY);
    retry_count++;
  }

  // Reset retry count for xApp stopping
  retry_count = 0;

  // Try to stop xApp with a maximum number of retries
  while (!stopped && retry_count < MAX_RETRIES) {
    stopped = try_stop_xapp_api();
    if (!stopped) {
      usleep(RETRY_DELAY);
      retry_count++;
    }
  }

  if (!stopped) {
    printf("[xApp]: Failed to stop cleanly after %d attempts\n", MAX_RETRIES);
    // Force cleanup
    if (xapp != NULL) {
      free_e42_xapp(xapp);
      xapp = NULL;
    }
    exit(EXIT_FAILURE);
  } else {
    printf("[xApp]: Stopped successfully after %d attempts\n", retry_count + 1);
    exit(EXIT_SUCCESS);
  }
}

static inline void* static_start_xapp(void* a)
{
  (void)a;
  // Blocking...
  start_e42_xapp(xapp);
  return NULL;
}

void init_xapp_api(fr_args_t const* args)
{
  assert(xapp == NULL && "The init_xapp_api function can only be called once");
  assert(args != NULL);

  // Signal handler
  signal(SIGINT, sig_handler);

  xapp = init_e42_xapp(args);

  // Spawn a new thread for the xapp
  int rc = pthread_create(&thrd_xapp, NULL, static_start_xapp, NULL);
  assert(rc == 0);

  while (connected_e42_xapp(xapp) == false)
    sleep(1);
}

bool try_stop_xapp_api(void)
{
  assert(xapp != NULL);

  size_t sz = not_dispatch_msg(xapp);
  if (sz > 0) {
    printf("[xApp]: Still has %zu pending messages\n", sz);
    return false;
  }

  free_e42_xapp(xapp);
  xapp = NULL;

  int const rc = pthread_join(thrd_xapp, NULL);
  assert(rc == 0);
  printf("[xApp]: Sucessfully stopped \n");
  return true;
}

e2_node_arr_xapp_t e2_nodes_xapp_api(void)
{
  assert(xapp != NULL);

  return e2_nodes_xapp(xapp);
}

/*
static inline
bool valid_interval(inter_xapp_e i)
{
  assert(i == ms_1
        || i == ms_1
        || i == ms_2
        || i == ms_5
        || i == ms_10
        || i == ms_100
        || i == ms_1000
      );

  return true;
}
*/

static inline bool valid_global_e2_node(global_e2_node_id_t* id, reg_e2_nodes_t* n)
{
  assert(id != NULL);
  assert(n != NULL);

  assoc_rb_tree_t t = cp_reg_e2_node(n);
  defer({ assoc_free(&t); };);

  void* it = assoc_front(&t);
  void* end = assoc_end(&t);

  it = find_if(&t, it, end, id, eq_global_e2_node_id_wrapper);

  return it == end ? false : true;
}

static inline bool valid_sm_id(global_e2_node_id_t* id, uint32_t sm_id)
{
  assert(id != NULL);

  // Only for testing purposes
  assert(sm_id == 2 || sm_id == 3 || sm_id == 142 || sm_id == 143 || sm_id == 144 || sm_id == 145 || sm_id == 146 || sm_id == 147
         || sm_id == 148);

  return true;
}

// returns a handle
sm_ans_xapp_t report_sm_xapp_api(global_e2_node_id_t* id, uint32_t rf_id, void* data, sm_cb handler)
{
  assert(xapp != NULL);
  assert(id != NULL);
  assert(data != NULL);

  assert(valid_global_e2_node(id, &xapp->e2_nodes) == true);
  assert(valid_sm_id(id, rf_id) == true);

  return report_sm_sync_xapp(xapp, id, rf_id, data, handler);
}

// remove the handle previously returned
void rm_report_sm_xapp_api(int const handle)
{
  assert(xapp != NULL);
  assert(handle > -1);

  // printf("Remove handle number = %d \n", handle);
  rm_report_sm_sync_xapp(xapp, handle);
}

sm_ans_xapp_t control_sm_xapp_api(global_e2_node_id_t* id, uint32_t ran_func_id, void* wr)
{
  assert(xapp != NULL);
  assert(id != NULL);
  assert(ran_func_id == SM_MAC_ID || ran_func_id == SM_SLICE_ID || ran_func_id == SM_TC_ID || ran_func_id == SM_RC_ID);
  assert(wr != NULL);

  return control_sm_sync_xapp(xapp, id, ran_func_id, wr);
}
