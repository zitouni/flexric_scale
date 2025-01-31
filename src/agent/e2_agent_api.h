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

#ifndef E2_AGENT_API_MOSAIC_H
#define E2_AGENT_API_MOSAIC_H

#include "../sm/sm_io.h"
#include "../util/conf_file.h"
#include "../util/ngran_types.h"

#include <pthread.h>
#include <stdbool.h>

// Forward declaration of the opaque type
typedef struct agent_instance_s agent_instance_t;

// Structure definitions needed by clients
// struct plugin_ag_s {
//   pthread_mutex_t sm_ds_mtx;
//   void* sm_ds;
// };

// Opaque type for plugin
// typedef struct plugin_wrapper {
//   pthread_mutex_t mtx;
//   void* data;
//   bool mutex_initialized; // Flag to track mutex initialization
// } plugin_wrapper_t;

// Getter functions for accessing agent instances
agent_instance_t* get_agent_instance(int index);
void lock_agents_mutex(void);
void unlock_agents_mutex(void);

// Accessor functions for agent instance members
bool is_agent_active(agent_instance_t* instance);
void* get_instance_agent(agent_instance_t* instance);
const char* get_instance_ric_ip(agent_instance_t* instance);

void* get_agent_plugin(void* e2_agent);
// void* get_plugin_data(plugin_wrapper_t* plugin);
// bool init_plugin_mutex(plugin_wrapper_t* plugin);
// void cleanup_plugin_wrapper(plugin_wrapper_t* plugin);
// void* get_sm_from_tree(plugin_wrapper_t* plugin, int model_id);

void init_agent_api(int mcc,
                    int mnc,
                    int mnc_digit_len,
                    int nb_id,
                    int cu_du_id,
                    ngran_node_t ran_type,
                    sm_io_ag_ran_t io,
                    fr_args_t const* args);

void stop_agent_api(void);

void async_event_agent_api(uint32_t ric_req_id, void* ind_data);

// Only expose what's needed for the RRC about the handover
void send_ho_completion_indication();

#endif
