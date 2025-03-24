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

#include "asio_agent.h"
#include "e2_agent.h"
#include "endpoint_agent.h"
#include "msg_handler_agent.h"
#include "not_handler_agent.h"
#include "lib/async_event.h"
#include "lib/e2ap/e2ap_ap_wrapper.h"
#include "lib/e2ap/e2ap_msg_free_wrapper.h"
#include "gen_msg_agent.h"
#include "util/alg_ds/alg/alg.h"
#include "util/alg_ds/ds/lock_guard/lock_guard.h"
#include "util/compare.h"

#include "../../../RAN_FUNCTION/surrey_log.h"

#include <byteswap.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>

static ric_indication_t generate_aindication(e2_agent_t* ag, sm_ind_data_t* data, aind_event_t* ai_ev)
{
  assert(ag != NULL);
  assert(data != NULL);
  assert(ai_ev != NULL);

  ric_indication_t ind = {.ric_id = ai_ev->ric_id, .action_id = ai_ev->action_id, .sn = NULL, .type = RIC_IND_REPORT};

  ind.hdr.len = data->len_hdr;
  ind.hdr.buf = data->ind_hdr;
  ind.msg.len = data->len_msg;
  ind.msg.buf = data->ind_msg;
  if (data->call_process_id != NULL) {
    ind.call_process_id = malloc(sizeof(*ind.call_process_id));
    assert(ind.call_process_id != NULL && "Memory exhausted");
    ind.call_process_id->buf = data->call_process_id;
    ind.call_process_id->len = data->len_cpid;
  }
  return ind;
}

static ric_indication_t generate_indication(e2_agent_t* ag, sm_ind_data_t* data, ind_event_t* i_ev)
{
  assert(ag != NULL);
  assert(data != NULL);
  assert(i_ev != NULL);

  ric_indication_t ind = {.ric_id = i_ev->ric_id, .action_id = i_ev->action_id, .sn = NULL, .type = RIC_IND_REPORT};

  ind.hdr.len = data->len_hdr;
  ind.hdr.buf = data->ind_hdr;
  ind.msg.len = data->len_msg;
  ind.msg.buf = data->ind_msg;
  if (data->call_process_id != NULL) {
    ind.call_process_id = malloc(sizeof(*ind.call_process_id));
    assert(ind.call_process_id != NULL && "Memory exhausted");
    ind.call_process_id->buf = data->call_process_id;
    ind.call_process_id->len = data->len_cpid;
  }
  return ind;
}

static inline void free_fd(void* key, void* value)
{
  assert(key != NULL);
  assert(value != NULL);
  int* fd = (int*)key;
  assert(*fd > 0);
  free(value);
}

static inline void free_pending_ev(void* key, void* value)
{
  assert(key != NULL);
  assert(value != NULL);
  pending_event_t* ev = (pending_event_t*)key;
  assert(valid_pending_event(*ev));
  free(value);
}

static inline void free_pending_agent(e2_agent_t* ag)
{
  assert(ag != NULL);
  bi_map_free(&ag->pending);
}

static inline void free_indication_event(e2_agent_t* ag)
{
  assert(ag != NULL);
  bi_map_free(&ag->ind_event);
  pthread_mutex_destroy(&ag->mtx_ind_event);
}

static inline void init_pending_events(e2_agent_t* ag)
{
  assert(ag != NULL);
  size_t fd_sz = sizeof(int);
  size_t event_sz = sizeof(pending_event_t);
  bi_map_init(&ag->pending, fd_sz, event_sz, cmp_fd, cmp_pending_event, free_fd, free_pending_ev);
}

static inline void free_ind_event_map(void* key, void* value)
{
  assert(key != NULL);
  assert(value != NULL);

  (void)key;

  ind_event_t* ev = (ind_event_t*)value;
  if (ev->sm->free_act_def != NULL)
    ev->sm->free_act_def(ev->sm, ev->act_def);

  free(ev);
}

static inline void free_key(void* key, void* value)
{
  assert(key != NULL);
  assert(value != NULL);

  (void)key;

  int* fd = (int*)value;
  free(fd);
}

static inline void init_indication_event(e2_agent_t* ag)
{
  assert(ag != NULL);
  size_t key_sz_fd = sizeof(int);
  size_t key_sz_ind = sizeof(ind_event_t);

  bi_map_init(&ag->ind_event, key_sz_fd, key_sz_ind, cmp_fd, cmp_ind_event, free_ind_event_map, free_key);

  pthread_mutexattr_t attr = {0};
#ifdef DEBUG
  pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK);
#endif
  int rc = pthread_mutex_init(&ag->mtx_ind_event, &attr);
  assert(rc == 0);
}

static inline void* ind_fd(e2_agent_t* ag, int fd)
{
  assert(ag != NULL);
  assert(fd > 0);

  void* start_it = assoc_front(&ag->ind_event.left);
  void* end_it = assoc_end(&ag->ind_event.left);

  void* it = find_if(&ag->ind_event.left, start_it, end_it, &fd, eq_fd);
  return it;
}

static inline bool net_pkt(const e2_agent_t* ag, int fd)
{
  assert(ag != NULL);
  assert(fd > 0);
  return fd == ag->ep.base.fd;
}

static inline bool ind_event(e2_agent_t* ag, int fd, ind_event_t** i_ev)
{
  assert(*i_ev == NULL);
  void* it = ind_fd(ag, fd);
  void* end_it = assoc_end(&ag->ind_event.left); // bi_map_end_left(&ag->ind_event);
  if (it != end_it) {
    *i_ev = assoc_value(&ag->ind_event.left, it);
    return true;
  }
  return false;
}

static inline bool aind_event(e2_agent_t* ag, int fd, arr_aind_event_t* dst)
{
  if (fd != ag->io.pipe.r)
    return false;

  const size_t sz = size_tsq(&ag->aind);
  assert(sz > 0 && "Aperiodic Event detected but the queue is empty!");
  dst->len = sz;
  dst->arr = calloc(sz, sizeof(aind_event_t));
  assert(dst->arr != NULL && "Memory exhausted");

  for (size_t i = 0; i < sz; ++i)
    pop_tsq(&ag->aind, &dst->arr[i], sizeof(aind_event_t));

  return true;
}

static inline bool pend_event(e2_agent_t* ag, int fd, pending_event_t** p_ev)
{
  assert(ag != NULL);
  assert(fd > 0);
  assert(*p_ev == NULL);

  // Remove assertion that requires exactly one pending event
  // assert(bi_map_size(&ag->pending) == 1);

  // Check if there are any pending events
  if (bi_map_size(&ag->pending) == 0) {
    return false;
  }

  void* start_it = assoc_front(&ag->pending.left);
  void* end_it = assoc_end(&ag->pending.left);

  if (!start_it || !end_it) {
    printf("[E2-AGENT]: Invalid pending event iterators\n");
    return false;
  }

  void* it = find_if(&ag->pending.left, start_it, end_it, &fd, eq_fd);

  if (it == end_it) {
    printf("[E2-AGENT]: No matching pending event found for fd %d\n", fd);
    return false;
  }
  *p_ev = assoc_value(&ag->pending.left, it);
  return *p_ev != NULL;
}

static void consume_fd_sync(int fd)
{
  assert(fd > 0);
  uint64_t read_buf = 0;
  ssize_t const bytes = read(fd, &read_buf, sizeof(read_buf));
  // assert(bytes == -1);
  assert(bytes <= (ssize_t)sizeof(read_buf));
}

static int consume_fd_async(int fd)
{
  assert(fd > 0);
  uint32_t read_buf = 0;
  ssize_t const bytes = read(fd, &read_buf, sizeof(read_buf));
  // assert(bytes == 4);
  // printf("Consumed %u \n", read_buf);
  return bytes;
}

static async_event_t next_async_event_agent(e2_agent_t* ag)
{
  assert(ag != NULL);

  int const fd = event_asio_agent(&ag->io);

  async_event_t e = {.type = UNKNOWN_EVENT, .fd = fd};

  if (fd == -1) { // no event happened. Just for checking the stop_token condition
    e.type = CHECK_STOP_TOKEN_EVENT;

  } else if (net_pkt(ag, fd) == true) {
    e.msg = e2ap_recv_msg_agent(&ag->ep);
    if (e.msg.type == SCTP_MSG_NOTIFICATION) {
      e.type = SCTP_CONNECTION_SHUTDOWN_EVENT;
      printf("[E2-AGENT]: SCTP Connection shutdown detected\n");

    } else if (e.msg.type == SCTP_MSG_PAYLOAD) {
      e.type = SCTP_MSG_ARRIVED_EVENT;

    } else {
      assert(0 != 0 && "Unknown type");
    }

  } else if (aind_event(ag, fd, &e.ai_ev) == true) {
    e.type = APERIODIC_INDICATION_EVENT;

  } else if (ind_event(ag, fd, &e.i_ev) == true) {
    e.type = INDICATION_EVENT;

  } else if (pend_event(ag, fd, &e.p_ev) == true) {
    e.type = PENDING_EVENT;

  } else {
    assert(0 != 0 && "Unknown event happened!");
  }
  return e;
}

void print_indication_content(const ric_indication_t* ind)
{
  if (!ind) {
    printf("Indication pointer is NULL\n");
    return;
  }

  printf("\n=== Indication Message Content ===\n");
  printf("RIC Request ID: %d\n", ind->ric_id.ric_req_id);
  printf("RAN Function ID: %d\n", ind->ric_id.ran_func_id);
  printf("Action ID: %d\n", ind->action_id);

  // Print message content
  if (ind->msg.buf != NULL) {
    printf("Message Content (%zu bytes):\n", ind->msg.len);
    for (size_t i = 0; i < ind->msg.len; i++) {
      printf("%02x ", ind->msg.buf[i]);
      if ((i + 1) % 16 == 0)
        printf("\n");
    }
    printf("\n");
  } else {
    printf("Message buffer is empty\n");
  }
  printf("===========================\n\n");
}

// static void print_byte_array_simple(const byte_array_t* ba)
// {
//   printf("At print bytes Encoded Message ba size (%zu bytes):\n", ba->len);
//   if (ba->buf != NULL) {
//     printf("Encoded Message ba Content (%zu bytes):\n", ba->len);
//     for (size_t i = 0; i < ba->len; i++) {
//       printf("%02x ", ba->buf[i]);
//       if ((i + 1) % 16 == 0)
//         printf("\n");
//     }
//     printf("\n");
//   } else {
//     printf("Encoded message ba buffer is empty\n");
//   }
// }

gtp_ho_info_t extract_handover_info(const uint8_t* buffer, size_t len)
{
  gtp_ho_info_t ho_info = {0}; // Initialize to zero

  // Check if buffer has enough bytes (12 bytes for uint32_t's + 1 for bool)
  if (buffer == NULL || len < 13) {
    printf("Error: Invalid buffer or insufficient length\n");
    return ho_info;
  }

  // Get pointer to start of handover info (last 13 bytes)
  const uint8_t* last_bytes = buffer + len - 13;

  // Copy all uint32_t values at once (12 bytes)
  memcpy(&ho_info, last_bytes, 12); // Copy ue_id, source_du, and target_du

  // Copy the bool separately since it's the last byte
  ho_info.ho_complete = last_bytes[12];

  return ho_info;
}

void print_exp_indication_data(exp_ind_data_t* exp, const char* prefix)
{
  LOG_SURREY_E2AGENT("\n=== %s Indication Data ===\n", prefix);

  if (exp->has_value && exp->data.ind_msg != NULL) {
    LOG_SURREY_E2AGENT("\nMessage Data (%zu bytes):\n", exp->data.len_msg);

    // Get raw bytes
    uint8_t* raw_bytes = (uint8_t*)exp->data.ind_msg;

    // Print raw bytes
    // LOG_SURREY_E2AGENT("Raw bytes:\n");
    // for (size_t i = 0; i < exp->data.len_msg; i++) {
    //   printf("%02x ", raw_bytes[i]);
    //   if ((i + 1) % 16 == 0)
    //     printf("\n");
    // }
    // printf("\n");

    // Extract handover info
    gtp_ho_info_t ho_info = extract_handover_info(raw_bytes, exp->data.len_msg);

    // Update the structure
    gtp_ind_msg_t* msg = (gtp_ind_msg_t*)exp->data.ind_msg;
    msg->ho_info = ho_info;

    // Print the results
    LOG_SURREY_E2AGENT("Extracted Handover Info:\n");
    LOG_SURREY_E2AGENT("UE %u (Source DU: %u, Target DU: %u, HO Complete: %s)\n",
                       ho_info.ue_id,
                       ho_info.source_du,
                       ho_info.target_du,
                       ho_info.ho_complete ? "true" : "false");
  }
  LOG_SURREY_E2AGENT("\n===========================\n");
}

void print_sm_ind_data(sm_ind_data_t* ind_data, const char* prefix)
{
  LOG_SURREY_E2AGENT("\n=== %s SM Indication Data ===\n", prefix);

  if (ind_data != NULL) {
    // Print header if exists
    if (ind_data->ind_hdr != NULL) {
      LOG_SURREY_E2AGENT("\nHeader Data (%zu bytes):\n", ind_data->len_hdr);
      for (size_t i = 0; i < ind_data->len_hdr; i++) {
        printf("%02x ", ind_data->ind_hdr[i]);
        if ((i + 1) % 16 == 0)
          printf("\n");
      }
      printf("\n");
    } else {
      LOG_SURREY_E2AGENT("No header data\n");
    }

    // Print message if exists
    if (ind_data->ind_msg != NULL) {
      LOG_SURREY_E2AGENT("\nMessage Data (%zu bytes):\n", ind_data->len_msg);

      // Print GTP Handover Info
      gtp_ind_msg_t* msg = (gtp_ind_msg_t*)ind_data->ind_msg;
      LOG_SURREY_E2AGENT("GTP Handover Info: UE %u (Source DU: %u, Target DU: %u)\n",
                         msg->ho_info.ue_id,
                         msg->ho_info.source_du,
                         msg->ho_info.target_du);

      // Print raw bytes
      LOG_SURREY_E2AGENT("Raw bytes:\n");
      uint8_t* raw_bytes = ind_data->ind_msg;
      for (size_t i = 0; i < ind_data->len_msg; i++) {
        printf("%02x ", raw_bytes[i]);
        if ((i + 1) % 16 == 0)
          printf("\n");
      }
      printf("\n");
    } else {
      LOG_SURREY_E2AGENT("No message data\n");
    }

    // Print call process ID if exists
    if (ind_data->call_process_id != NULL) {
      LOG_SURREY_E2AGENT("\nCall Process ID (%zu bytes):\n", ind_data->len_cpid);
      for (size_t i = 0; i < ind_data->len_cpid; i++) {
        printf("%02x ", ind_data->call_process_id[i]);
        if ((i + 1) % 16 == 0)
          printf("\n");
      }
      printf("\n");
    } else {
      LOG_SURREY_E2AGENT("No call process ID\n");
    }
  }
  printf("===============================\n\n");
}

static void handle_pending_event(e2_agent_t* ag)
{
  assert(ag != NULL);

  // Find current RIC connection
  ric_connection_t* current_ric = NULL;
  // int current_ric_index = -1;

  for (int i = 0; i < ag->num_rics; i++) {
    if (strcmp(ag->init_ric_addr, ag->ric_connections[i].ric_addr) == 0) {
      current_ric = &ag->ric_connections[i];
      // current_ric_index = i;
      break;
    }
  }

  if (!current_ric) {
    printf("[E2-AGENT]: No RIC connection found for %s\n", ag->init_ric_addr);
    return;
  }

  // Lock current RIC's mutex
  pthread_mutex_lock(&current_ric->mtx);

  // Clear any existing pending events and timers
  void* start_it = assoc_front(&ag->pending.left);
  void* end_it = assoc_end(&ag->pending.left);
  while (start_it != end_it) {
    int* fd_ptr = (int*)assoc_key(&ag->pending.left, start_it);
    if (fd_ptr) {
      close(*fd_ptr);
    }
    start_it = assoc_next(&ag->pending.left, start_it);
  }
  bi_map_free(&ag->pending);
  init_pending_events(ag);

  current_ric->retry_count++;
  printf("[E2-AGENT]: E2 SETUP REQUEST attempt for RIC %s (attempt %d)\n", current_ric->ric_addr, current_ric->retry_count);

  // Create single new timer with 1 second interval
  int new_timer = create_timer_ms_asio_agent(&ag->io, 1000, 0); // One-shot timer
  if (new_timer < 0) {
    printf("[E2-AGENT]: Failed to create timer. Waiting before retry...\n");
    pthread_mutex_unlock(&current_ric->mtx);
    usleep(1000000); // Wait 1 second before retry
    return;
  }

  // Set up pending event for next attempt
  pending_event_t new_ev = SETUP_REQUEST_PENDING_EVENT;
  bi_map_insert(&ag->pending, &new_timer, sizeof(new_timer), &new_ev, sizeof(new_ev));

  // Send setup request
  e2_setup_request_t sr = gen_setup_request(&ag->ap.version.type, ag);
  byte_array_t ba = e2ap_enc_setup_request_ag(&ag->ap, &sr);

  if (ba.buf && ba.len > 0) {
    e2ap_send_bytes_agent(&ag->ep, ba);
    printf("[E2-AGENT]: Sent setup request to RIC %s\n", ag->init_ric_addr);
  } else {
    printf("[E2-AGENT]: Failed to encode setup request\n");
    close(new_timer);
    bi_map_free(&ag->pending);
    init_pending_events(ag);
  }

  free_byte_array(ba);
  e2ap_free_setup_request(&sr);
  pthread_mutex_unlock(&current_ric->mtx);
  // Ensure we consume any pending timer events
  consume_fd_sync(new_timer);
}

static void handle_connection_shutdown(e2_agent_t* ag)
{
  assert(ag != NULL);
  printf("[E2-AGENT]: Handling RIC disconnection...\n");

  // Set connection state
  ag->connection_state = DISCONNECTED;

  // Wait briefly before attempting reconnection
  usleep(100000); // 100ms

  // This part to resume pending after disconnection of a near-RT RIC instance
  pthread_mutex_lock(&ag->mtx_pending);

  // Clear existing pending events
  if (bi_map_size(&ag->pending) > 0) {
    bi_map_free(&ag->pending);
  }
  init_pending_events(ag);

  // Create new timer with same timing as initial startup (3 seconds)
  int fd_timer = create_timer_ms_asio_agent(&ag->io, 3000, 3000);
  if (fd_timer >= 0) {
    // Set up new pending event
    pending_event_t ev = SETUP_REQUEST_PENDING_EVENT;
    bi_map_insert(&ag->pending, &fd_timer, sizeof(fd_timer), &ev, sizeof(ev));
    printf("[E2-AGENT]: Scheduled new setup request after disconnection (timer fd: %d)\n", fd_timer);
  } else {
    printf("[E2-AGENT]: Failed to create timer after disconnection\n");
  }

  pthread_mutex_unlock(&ag->mtx_pending);
}

static void e2_event_loop_agent(e2_agent_t* ag)
{
  assert(ag != NULL);
  while (ag->stop_token == false) {
    async_event_t e = next_async_event_agent(ag);
    assert(e.type != UNKNOWN_EVENT && "Unknown event triggered ");

    switch (e.type) {
      case SCTP_MSG_ARRIVED_EVENT: {
        defer({ free_sctp_msg(&e.msg); });

        e2ap_msg_t msg = e2ap_msg_dec_ag(&ag->ap, e.msg.ba);
        defer({ e2ap_msg_free_ag(&ag->ap, &msg); });

        e2ap_msg_t ans = e2ap_msg_handle_agent(ag, &msg);
        defer({ e2ap_msg_free_ag(&ag->ap, &ans); });

        if (ans.type != NONE_E2_MSG_TYPE) {
          byte_array_t ba_ans = e2ap_msg_enc_ag(&ag->ap, &ans);
          defer({ free_byte_array(ba_ans); });

          e2ap_send_bytes_agent(&ag->ep, ba_ans);
        }

        break;
      }
      case APERIODIC_INDICATION_EVENT: {
        arr_aind_event_t* aind = &e.ai_ev;
        assert(aind->len > 0 && aind->arr != NULL);
        defer({ free(aind->arr); });
        for (size_t i = 0; i < aind->len; ++i) {
          sm_agent_t const* sm = aind->arr[i].sm;
          sm_ind_data_t* ind_data = aind->arr[i].ind_data;

          if (sm == NULL) {
            printf("Error: sm (Service Model) pointer is NULL\n");
            return; // Return empty structure
          }

          if (sm->proc.on_indication == NULL) {
            printf("Error: on_indication callback is not initialized\n");
            return;
          }

          if (ind_data == NULL) {
            printf("Error: indication data is NULL\n");
            return;
          }

          // Add debug prints for Surrey HiperRAN

          // LOG_SURREY("e2_event_loop_agent: SM ID: %d\n", sm->info.id());
          // LOG_SURREY("e2_event_loop_agent: SM Agent pointer @: %p\n", (void*)sm);
          // LOG_SURREY("e2_event_loop_agent: ind_data pointer @: %p\n", (void*)ind_data);

          // print_sm_ind_data(ind_data, "Before");
          exp_ind_data_t exp = sm->proc.on_indication(sm, ind_data); // , &e.i_ev->ric_id);
                                                                     // Add handover info print here

          // Print the results
          // After this function the handover payload information is integrated
          print_exp_indication_data(&exp, "After");

          // Condition not matched e.g., No UE matches condition
          if (exp.has_value == false) {
            int rc = consume_fd_async(ag->io.pipe.r);
            assert(rc != 1 && "No bytes in the pipe but message in the queue! ");
            continue;
          }

          ric_indication_t ind = generate_aindication(ag, &exp.data, &aind->arr[i]);
          defer({ e2ap_free_indication(&ind); });

          // print_indication_content(&ind);

          byte_array_t ba = e2ap_enc_indication_ag(&ag->ap, &ind);

          defer({ free_byte_array(ba); });

          e2ap_send_bytes_agent(&ag->ep, ba);

          int rc = consume_fd_async(ag->io.pipe.r);
          assert(rc != 1 && "No bytes in the pipe but message in the queue! ");

          exit(0);
        }
        break;
      }
      case INDICATION_EVENT: {
        sm_agent_t const* sm = e.i_ev->sm;
        void* act_def = e.i_ev->act_def;
        exp_ind_data_t exp = sm->proc.on_indication(sm, act_def); // , &e.i_ev->ric_id);
        // Condition not matched e.g., No UE matches condition
        if (exp.has_value == false) {
          printf(
              "[E2 AGENT]: Condition not matched e.g., No UE matches condition. Emulator triggers this condition for testing, "
              "but "
              "not the RAN \n");
          consume_fd_sync(e.fd);
          break;
        }
        ric_indication_t ind = generate_indication(ag, &exp.data, e.i_ev);
        defer({ e2ap_free_indication(&ind); });

        byte_array_t ba = e2ap_enc_indication_ag(&ag->ap, &ind);
        defer({ free_byte_array(ba); });

        e2ap_send_bytes_agent(&ag->ep, ba);

        consume_fd_sync(e.fd);

        break;
      }
      case PENDING_EVENT: {
        if (!ag || !e.p_ev || e.fd <= 0) {
          if (e.fd > 0)
            close(e.fd);
          // consume_fd_sync(e.fd);
          break;
        }
        // Clean up the current timer
        close(e.fd);
        handle_pending_event(ag);
        break;
      }
      case SCTP_CONNECTION_SHUTDOWN_EVENT: {
        // First check if agent is valid
        if (!ag) {
          printf("[E2-AGENT]: Warning - Invalid agent during shutdown\n");
          break;
        }

        printf("[E2-AGENT]: Communication with the nearRT-RIC lost\n");
        handle_connection_shutdown(ag);
        // Handle notification and free message
        if (&e.msg) {
          // notification_handle_ag(ag, &e.msg);
          free_sctp_msg(&e.msg);
        }

        break;
      }
      case CHECK_STOP_TOKEN_EVENT: {
        break;
      }
      default: {
        assert(0 != 0 && "Unknown event happened");
        if (ag->connection_state == DISCONNECTED) {
          // If disconnected, create new pending event
          handle_connection_shutdown(ag);
        }
        break;
      }
    }
  }

  printf("ag->agent_stopped = true \n");
  ag->agent_stopped = true;
}

e2_agent_t* e2_init_agent(const char* addr,
                          int port,
                          global_e2_node_id_t ge2nid,
                          sm_io_ag_ran_t io,
                          char const* libs_dir,
                          e2_agent_args_t* args)
{
  assert(addr != NULL);
  assert(port > 0 && port < 65535);
  assert(args != NULL);

  printf("[E2 AGENT]: Initializing ... \n");

  e2_agent_t* ag = calloc(1, sizeof(*ag));
  assert(ag != NULL && "Memory exhausted");

  // Store the args

  // Initialize RIC connections
  ag->num_rics = args->ric_ip_list.num_ric_addresses;
  ag->ric_connections = calloc(ag->num_rics, sizeof(ric_connection_t));

  for (int i = 0; i < ag->num_rics; i++) {
    ag->ric_connections[i].ric_addr = strdup(args->ric_ip_list.ric_ip_addresses[i]);
    pthread_mutex_init(&ag->ric_connections[i].mtx, NULL);
    ag->ric_connections[i].retry_count = 0;
    ag->ric_connections[i].active = true;
  }
  ag->args = args;
  ag->connection_state = DISCONNECTED;
  // Initialize mutex
  pthread_mutex_init(&ag->mtx_pending, NULL);

  ag->init_ric_addr = strdup(addr);

  // Initialize endpoint address using memcpy

  e2ap_init_ep_agent(&ag->ep, addr, port);

  init_asio_agent(&ag->io);

  add_fd_asio_agent(&ag->io, ag->ep.base.fd);

  init_ap(&ag->ap.base.type);

  ag->sz_handle_msg = sizeof(ag->handle_msg) / sizeof(ag->handle_msg[0]);
  init_handle_msg_agent(ag->sz_handle_msg, &ag->handle_msg);

  init_plugin_ag(&ag->plugin, libs_dir, io);

  init_pending_events(ag);

  init_indication_event(ag);

  init_tsq(&ag->aind, sizeof(aind_event_t));

#if defined(E2AP_V2) || defined(E2AP_V3)
  // Read RAN
  assert(io.read_setup_ran != NULL);
  ag->read_setup_ran = io.read_setup_ran;

  ag->trans_id_setup_req = 0;
#endif

  ag->global_e2_node_id = ge2nid;
  ag->stop_token = false;
  ag->agent_stopped = false;

  return ag;
}

void e2_start_agent(e2_agent_t* ag)
{
  assert(ag != NULL);

  // Validate initial state
  if (!ag->ep.base.addr) {
    printf("[E2-AGENT]: Invalid RIC address\n");
    return;
  }

  // Store initial RIC address
  ag->init_ric_addr = strdup(ag->ep.base.addr);
  if (!ag->init_ric_addr) {
    printf("[E2-AGENT]: Memory allocation failed\n");
    return;
  }

  // Set initial connection state
  ag->connection_state = DISCONNECTED;

  // Initialize mutex
  if (pthread_mutex_init(&ag->mtx_pending, NULL) != 0) {
    printf("[E2-AGENT]: Mutex initialization failed\n");
    free((void*)ag->init_ric_addr);
    ag->init_ric_addr = NULL;
    return;
  }

  pthread_mutex_lock(&ag->mtx_pending);

  // Initialize pending events
  if (bi_map_size(&ag->pending) > 0) {
    bi_map_free(&ag->pending);
  }
  init_pending_events(ag);

  // Create initial timer
  int fd_timer = create_timer_ms_asio_agent(&ag->io, 3000, 3000);
  if (fd_timer < 0) {
    printf("[E2-AGENT]: Timer creation failed\n");
    pthread_mutex_unlock(&ag->mtx_pending);
    pthread_mutex_destroy(&ag->mtx_pending);
    free((void*)ag->init_ric_addr);
    ag->init_ric_addr = NULL;
    return;
  }

  // Set up initial pending event
  pending_event_t ev = SETUP_REQUEST_PENDING_EVENT;
  bi_map_insert(&ag->pending, &fd_timer, sizeof(fd_timer), &ev, sizeof(ev));

  printf("[E2-AGENT]: Initial timer created with fd %d for RIC %s\n", fd_timer, ag->init_ric_addr);

  pthread_mutex_unlock(&ag->mtx_pending);

  // Generate and send initial setup request
  printf("[E2-AGENT]: Sending SETUP-REQUEST to RIC %s\n", ag->init_ric_addr);

  e2_setup_request_t sr = gen_setup_request(&ag->ap.version.type, ag);
  byte_array_t ba = e2ap_enc_setup_request_ag(&ag->ap, &sr);

  if (ba.buf && ba.len > 0) {
    e2ap_send_bytes_agent(&ag->ep, ba);
  } else {
    printf("[E2-AGENT]: Failed to generate setup request\n");
  }

  free_byte_array(ba);
  e2ap_free_setup_request(&sr);

  // Start event loop
  e2_event_loop_agent(ag);
}

void e2_free_agent(e2_agent_t* ag)
{
  if (ag == NULL)
    return;

  // Free args structure
  if (ag->args) {
    if (ag->args->client_ip) {
      free((void*)ag->args->client_ip); // Cast away const
    }
    if (ag->args->sm_dir) {
      free((void*)ag->args->sm_dir); // Cast away const
    }
    free(ag->args);
    // Add cleanup for ric_ip_addresses
    if (ag->args->ric_ip_list.ric_ip_addresses[0] != NULL) {
      free(ag->args->ric_ip_list.ric_ip_addresses[0]);
      ag->args->ric_ip_list.ric_ip_addresses[0] = NULL;
    }

    // Clean up RIC connections
    for (int i = 0; i < ag->num_rics; i++) {
      free(ag->ric_connections[i].ric_addr);
      pthread_mutex_destroy(&ag->ric_connections[i].mtx);
    }
    free(ag->ric_connections);
  }
  if (ag->init_ric_addr) {
    free((void*)ag->init_ric_addr);
    ag->init_ric_addr = NULL;
  }

  // Free endpoint address
  if (ag->ep.base.addr) {
    free((void*)ag->ep.base.addr);
  }

  ag->stop_token = true;
  while (ag->agent_stopped == false) {
    usleep(1000);
  }
  pthread_mutex_lock(&ag->mtx_pending);
  // Clear all pending events at shutdown
  if (bi_map_size(&ag->pending) > 0) {
    bi_map_free(&ag->pending);
  }
  pthread_mutex_unlock(&ag->mtx_pending);

  // Destroy mutex
  pthread_mutex_destroy(&ag->mtx_pending);

  free_plugin_ag(&ag->plugin);

  free_pending_agent(ag);

  free_indication_event(ag);

  free_tsq(&ag->aind, NULL);

  free_global_e2_node_id(&ag->global_e2_node_id);

  e2ap_free_ep_agent(&ag->ep);

  free(ag);
}

void e2_async_event_agent(e2_agent_t* ag, uint32_t ric_req_id, void* ind_data)
{
  assert(ag != NULL);

  void* f = NULL;
  void* l = NULL;
  void* it = NULL;

  ind_event_t tmp = {.ric_id.ric_req_id = ric_req_id, .sm = NULL, .action_id = 0};

  assoc_rb_tree_t* tree = &ag->ind_event.right;

  for (size_t i = 0; i < 10; ++i) {
    int rc = pthread_mutex_lock(&ag->mtx_ind_event);
    assert(rc == 0);

    f = assoc_rb_tree_front(tree);
    l = assoc_rb_tree_end(tree);
    it = find_if_rb_tree(tree, f, l, &tmp, eq_ind_event_ric_req_id);
    if (it != l)
      break;

    rc = pthread_mutex_unlock(&ag->mtx_ind_event);
    assert(rc == 0);

    // Give some time to propagate the subscription request and be sure
    // it has been writen in the ind_event ds.
    usleep(10);
  }

  assert(it != l && "Not found RIC Request ID");

  ind_event_t* ind_ev = assoc_rb_tree_key(tree, it);

  aind_event_t aind = {.ric_id = ind_ev->ric_id, .sm = ind_ev->sm, .action_id = ind_ev->action_id, .ind_data = ind_data};

  int rc = pthread_mutex_unlock(&ag->mtx_ind_event);
  assert(rc == 0);

  // Push the data into the queue
  push_tsq(&ag->aind, &aind, sizeof(aind_event_t));

  // Inform epoll that an aperiodic event happened
  int const num_char = 32;
  char str[num_char];
  memset(str, '\0', num_char);
  rc = snprintf(str, num_char, "%u", ric_req_id);
  assert(rc > 0 && rc < num_char - 1);

  rc = write(ag->io.pipe.w, str, rc);
  assert(rc != 0);
}

//////////////////////////////////
/////////////////////////////////

void e2_send_subscription_response(e2_agent_t* ag, const ric_subscription_response_t* sr)
{
  assert(ag != NULL);
  assert(sr != NULL);

  byte_array_t ba = e2ap_enc_subscription_response_ag(&ag->ap, sr);
  e2ap_send_bytes_agent(&ag->ep, ba);
  free_byte_array(ba);
}

void e2_send_subscription_failure(e2_agent_t* ag, const ric_subscription_failure_t* sf)
{
  assert(ag != NULL);
  assert(sf != NULL);

  byte_array_t ba = e2ap_enc_subscription_failure_ag(&ag->ap, sf);
  e2ap_send_bytes_agent(&ag->ep, ba);
  free_byte_array(ba);
}

void e2_send_indication_agent(e2_agent_t* ag, const ric_indication_t* indication)
{
  assert(ag != NULL);
  assert(indication != NULL);

  byte_array_t ba = e2ap_enc_indication_ag(&ag->ap, indication);
  e2ap_send_bytes_agent(&ag->ep, ba);
  free_byte_array(ba);
}

void e2_send_subscription_delete_response(e2_agent_t* ag, const ric_subscription_delete_response_t* sdr)
{
  assert(ag != NULL);
  assert(sdr != NULL);
  byte_array_t ba = e2ap_enc_subscription_delete_response_ag(&ag->ap, sdr);
  e2ap_send_bytes_agent(&ag->ep, ba);
  free_byte_array(ba);
}

void e2_send_subscription_delete_failure(e2_agent_t* ag, const ric_subscription_delete_failure_t* sdf)
{
  byte_array_t ba = e2ap_enc_subscription_delete_failure_ag(&ag->ap, sdf);
  e2ap_send_bytes_agent(&ag->ep, ba);
  free_byte_array(ba);
}

void e2_send_control_acknowledge(e2_agent_t* ag, const ric_control_acknowledge_t* ca)
{
  byte_array_t ba = e2ap_enc_control_acknowledge_ag(&ag->ap, ca);
  e2ap_send_bytes_agent(&ag->ep, ba);
  free_byte_array(ba);
}

void e2_send_control_failure(e2_agent_t* ag, const ric_control_failure_t* cf)
{
  byte_array_t ba = e2ap_enc_control_failure_ag(&ag->ap, cf);
  e2ap_send_bytes_agent(&ag->ep, ba);
  free_byte_array(ba);
}
