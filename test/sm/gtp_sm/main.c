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

#include "../../rnd/fill_rnd_data_gtp.h"
#include "../../../src/sm/gtp_sm/gtp_sm_agent.h"
#include "../../../src/sm/gtp_sm/gtp_sm_ric.h"
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

static gtp_ind_data_t cp;

/////
// AGENT
////

static bool read_gtp_RAN(void* read)
{
  assert(read != NULL);

  gtp_ind_data_t* gtp = (gtp_ind_data_t*)read;

  fill_gtp_ind_data(gtp);
  cp.hdr = cp_gtp_ind_hdr(&gtp->hdr);
  cp.msg = cp_gtp_ind_msg(&gtp->msg);
  return true;
}

/////////////////////////////
// Check Functions
// //////////////////////////

static void check_eq_ran_function(sm_agent_t const* ag, sm_ric_t const* ric)
{
  assert(ag != NULL);
  assert(ric != NULL);
  assert(ag->info.id() == ric->ran_func_id);
}

// RIC -> E2
static void check_subscription(sm_agent_t* ag, sm_ric_t* ric)
{
  assert(ag != NULL);
  assert(ric != NULL);

  char sub[] = "2_ms";
  sm_subs_data_t data = ric->proc.on_subscription(ric, &sub);

  sm_ag_if_ans_subs_t const subs = ag->proc.on_subscription(ag, &data);
  assert(subs.type == PERIODIC_SUBSCRIPTION_FLRC);
  assert(subs.per.t.ms == 2);

  free_sm_subs_data(&data);
}

// E2 -> RIC
static void check_indication(sm_agent_t* ag, sm_ric_t* ric)
{
  assert(ag != NULL);
  assert(ric != NULL);

  exp_ind_data_t exp = ag->proc.on_indication(ag, NULL);
  assert(exp.has_value == true);
  if (exp.data.call_process_id != NULL) {
    assert(exp.data.len_cpid != 0);
  }
  if (exp.data.ind_hdr != NULL) {
    assert(exp.data.len_hdr != 0);
  }
  if (exp.data.ind_msg != NULL) {
    assert(exp.data.len_msg != 0);
  }

  sm_ag_if_rd_ind_t msg = ric->proc.on_indication(ric, &exp.data);

  assert(msg.type == GTP_STATS_V0);

  gtp_ind_data_t* data = &msg.gtp;

  if (msg.gtp.msg.ngut != NULL) {
    assert(msg.gtp.msg.len != 0);
  }
  // Check handover information
  // Simple NULL check for ho_info data
  assert(&data->msg.ho_info != NULL);

  assert(eq_gtp_ind_hdr(&data->hdr, &cp.hdr) == true);
  assert(eq_gtp_ind_msg(&data->msg, &cp.msg) == true);
  assert(eq_gtp_call_proc_id(data->proc_id, cp.proc_id) == true);

  free_gtp_ind_hdr(&data->hdr);
  free_gtp_ind_msg(&data->msg);

  free_exp_ind_data(&exp);
}

int main()
{
  sm_io_ag_ran_t io_ag = {0};
  io_ag.read_ind_tbl[GTP_STATS_V0] = read_gtp_RAN;

  sm_agent_t* sm_ag = make_gtp_sm_agent(io_ag);

  sm_ric_t* sm_ric = make_gtp_sm_ric();

  check_eq_ran_function(sm_ag, sm_ric);
  check_subscription(sm_ag, sm_ric);
  check_indication(sm_ag, sm_ric);

  sm_ag->free_sm(sm_ag);
  sm_ric->free_sm(sm_ric);

  printf("Success\n");
  return EXIT_SUCCESS;
}
