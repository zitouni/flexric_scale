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

#include "e2_agent_api.h"

#include <assert.h> // for assert
#include <pthread.h> // for pthread_cr...
#include <stdlib.h>
#include <stdio.h> // for NULL
#include "e2_agent.h" // for e2_free_agent
#include "lib/e2ap/e2ap_global_node_id_wrapper.h" // for global_e2_...
#include "lib/e2ap/e2ap_plmn_wrapper.h" // for plmn_t
#include "util/ngran_types.h" // for ngran_gNB
#include "util/conf_file.h"

#define MAX_AGENTS 20

// static e2_agent_t* agent = NULL;

// static pthread_t thrd_agent;
// Structure to maintain information about each E2 agent instance
typedef struct {
  e2_agent_t* agent;
  pthread_t thread;
  bool active;
  char* ric_ip;
} agent_instance_t;

// Global variables to manage multiple agents
static agent_instance_t agents[MAX_AGENTS] = {0};
static int num_active_agents = 0;
static pthread_mutex_t agents_mutex = PTHREAD_MUTEX_INITIALIZER;

// Thread function that starts an E2 agent
static inline void* static_start_agent(void* arg)
{
  agent_instance_t* instance = (agent_instance_t*)arg;
  if (instance && instance->agent) {
    e2_start_agent(instance->agent);
  }
  return NULL;
}
// static inline void* static_start_agent(void* arg)
// {
//   (void)a;
//   // Blocking...
//   e2_start_agent(agent);
//   return NULL;
// }

static global_e2_node_id_t init_ge2ni(ngran_node_t ran_type, e2ap_plmn_t plmn, int nb_id, int cu_du_id)
{
  global_e2_node_id_t ge2ni = {.type = ran_type, .plmn = plmn, .nb_id.nb_id = nb_id, .nb_id.unused = 0, .cu_du_id = NULL};

  // NODE_IS_CU is an abuse, but there is no way in the standard to differentitate
  // between NODE_IS_GNB and NODE_IS_CU. Blame the standard
  if (NODE_IS_CU(ran_type) || NODE_IS_CUUP(ran_type) || NODE_IS_DU(ran_type)) {
    assert(cu_du_id > 0);
    ge2ni.cu_du_id = calloc(1, sizeof(uint64_t));
    assert(ge2ni.cu_du_id != NULL && "memory exhausted");
    *ge2ni.cu_du_id = cu_du_id;
  } else if (NODE_IS_MONOLITHIC(ran_type)) {
  } else {
    assert(0 != 0 && "not supported RAN type\n");
  }

  return ge2ni;
}

void init_agent_api(int mcc,
                    int mnc,
                    int mnc_digit_len,
                    int nb_id,
                    int cu_du_id,
                    ngran_node_t ran_type,
                    sm_io_ag_ran_t io,
                    fr_args_t const* args)
{
  // Validate input parameters
  //  assert(agent == NULL);
  assert(mcc > 0);
  assert(mnc > 0);
  assert(mnc_digit_len > 0);
  assert(nb_id > 0);
  assert(ran_type >= 0);

  // Lock mutex for thread safety

  pthread_mutex_lock(&agents_mutex);

  // Check if maximum agents limit reached
  if (num_active_agents >= MAX_AGENTS) {
    printf("[E2 AGENT]: Maximum number of agents (%d) reached\n", MAX_AGENTS);
    pthread_mutex_unlock(&agents_mutex);
    return;
  }

  // Get RIC IP address
  char* server_ip_str = get_near_ric_ip(args);
  if (!server_ip_str) {
    printf("[E2 AGENT]: Failed to get RIC IP address\n");
    pthread_mutex_unlock(&agents_mutex);
    return;
  }

  // Check if agent already exists for this IP
  for (int i = 0; i < num_active_agents; i++) {
    if (agents[i].active && agents[i].ric_ip && strcmp(agents[i].ric_ip, server_ip_str) == 0) {
      printf("[E2 AGENT]: Agent already exists for RIC IP %s\n", server_ip_str);
      free(server_ip_str);
      pthread_mutex_unlock(&agents_mutex);
      return;
    }
  }
  // Initialize PLMN and global node ID
  const e2ap_plmn_t plmn = {.mcc = mcc, .mnc = mnc, .mnc_digit_len = mnc_digit_len};
  global_e2_node_id_t ge2ni = init_ge2ni(ran_type, plmn, nb_id, cu_du_id);

  const int e2ap_server_port = 36421;

  // Log connection details
  char* ran_type_str = get_ngran_name(ran_type);
  char str[128] = {0};
  int it = sprintf(str,
                   "[E2 AGENT]: nearRT-RIC IP Address = %s, PORT = %d, RAN type = %s, nb_id = %d",
                   server_ip_str,
                   e2ap_server_port,
                   ran_type_str,
                   nb_id);
  assert(it > 0);

  if (ge2ni.cu_du_id != NULL) {
    it = sprintf(str + it, ", cu_du_id = %ld\n", *ge2ni.cu_du_id);
    assert(it > 0);
  } else {
    it = sprintf(str + it, "\n");
    assert(it > 0);
  }
  // assert(it < 128);
  printf("%s", str);

  // Initialize new agent instance
  agent_instance_t* instance = &agents[num_active_agents];
  instance->agent = e2_init_agent(server_ip_str, e2ap_server_port, ge2ni, io, args->libs_dir);

  // Check agent initialization
  if (instance->agent == NULL) {
    printf("[E2 AGENT]: Failed to initialize agent for RIC %s\n", server_ip_str);
    free(server_ip_str);
    pthread_mutex_unlock(&agents_mutex);
    return;
  }

  // Set instance parameters
  instance->active = true;
  instance->ric_ip = strdup(server_ip_str);

  // Create thread for this agent
  int rc = pthread_create(&instance->thread, NULL, static_start_agent, instance);
  if (rc != 0) {
    printf("[E2 AGENT]: Failed to create thread for RIC %s\n", server_ip_str);
    e2_free_agent(instance->agent);
    instance->active = false;
    free(instance->ric_ip);
    instance->ric_ip = NULL;
    free(server_ip_str);
    pthread_mutex_unlock(&agents_mutex);
    return;
  }

  // Increment active agents counter and cleanup

  num_active_agents++;
  free(server_ip_str);
  pthread_mutex_unlock(&agents_mutex);

  //   agent = e2_init_agent(server_ip_str, e2ap_server_port, ge2ni, io, args->libs_dir);

  // // Spawn a new thread for the agent
  // const int rc = pthread_create(&thrd_agent, NULL, static_start_agent, NULL);
  // assert(rc == 0);
  // free(server_ip_str);
}

void stop_agent_api(void)
{
  pthread_mutex_lock(&agents_mutex);

  // Stop and cleanup each active agent
  for (int i = 0; i < num_active_agents; i++) {
    if (agents[i].active) {
      e2_free_agent(agents[i].agent);
      int rc = pthread_join(agents[i].thread, NULL);
      assert(rc == 0);
      agents[i].active = false;
      if (agents[i].ric_ip) {
        free(agents[i].ric_ip);
        agents[i].ric_ip = NULL;
      }
    }
  }
  num_active_agents = 0;

  pthread_mutex_unlock(&agents_mutex);
  // assert(agent != NULL);
  // e2_free_agent(agent);
  // int const rc = pthread_join(thrd_agent, NULL);
  // assert(rc == 0);
}

void async_event_agent_api(uint32_t ric_req_id, void* ind_data)
{
  pthread_mutex_lock(&agents_mutex);

  // Send event to each active agent
  for (int i = 0; i < num_active_agents; i++) {
    if (agents[i].active && agents[i].agent) {
      e2_async_event_agent(agents[i].agent, ric_req_id, ind_data);
    }
  }

  pthread_mutex_unlock(&agents_mutex);
  // assert(agent != NULL);
  // e2_async_event_agent(agent, ric_req_id, ind_data);
}
