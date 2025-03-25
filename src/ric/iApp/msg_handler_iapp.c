/*
 * Licensed to the OpenAirInterface (OAI) Software Alliance under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The OpenAirInterface Software Alliance licenses this file to You under
 * the OAI Public License, Version 1.1  (the "License"); you may not use this file
 * except in compliance with the License.
a* You may obtain a copy of the License at
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

#include "../../xApp/e42_xapp_api.h"
#include "e42_iapp_api.h"
#include "msg_handler_iapp.h"
#include "lib/pending_events.h"
#include "util/alg_ds/alg/alg.h"
#include "util/compare.h"
#include "util/alg_ds/ds/lock_guard/lock_guard.h"
#include "util/time_now_us.h"

#include "iapp_if_generic.h"
#include "xapp_ric_id.h"

#include "sm/gtp_sm/gtp_sm_id.h"
#include "sm/gtp_sm/ie/gtp_data_ie.h"

#include "map_ric_id.h"
#include "../../util/alg_ds/ds/assoc_container/assoc_generic.h"

#include "../../../../RAN_FUNCTION/surrey_log.h"

#include <stdio.h>

static inline size_t next_pow2(size_t x)
{
  static_assert(sizeof(x) == 8, "Need this size to work correctly");
  x -= 1;
  x |= (x >> 1);
  x |= (x >> 2);
  x |= (x >> 4);
  x |= (x >> 8);
  x |= (x >> 16);
  x |= (x >> 32);

  return x + 1;
}

static bool check_valid_msg_type(e2_msg_type_t msg_type)
{
  return msg_type == RIC_SUBSCRIPTION_RESPONSE || msg_type == E42_SETUP_REQUEST || msg_type == E42_RIC_SUBSCRIPTION_REQUEST
         || msg_type == E42_RIC_SUBSCRIPTION_DELETE_REQUEST || msg_type == E42_RIC_CONTROL_REQUEST
         || msg_type == RIC_CONTROL_ACKNOWLEDGE || msg_type == RIC_INDICATION || msg_type == RIC_SUBSCRIPTION_DELETE_RESPONSE;
}

void init_handle_msg_iapp(size_t len, handle_msg_fp_iapp (*handle_msg)[len])
{
  assert(len == NONE_E2_MSG_TYPE);

  memset((*handle_msg), 0, sizeof(handle_msg_fp_iapp) * len);

  (*handle_msg)[RIC_SUBSCRIPTION_RESPONSE] = e2ap_handle_subscription_response_iapp;
  (*handle_msg)[E42_SETUP_REQUEST] = e2ap_handle_e42_setup_request_iapp;
  (*handle_msg)[E42_RIC_SUBSCRIPTION_REQUEST] = e2ap_handle_e42_ric_subscription_request_iapp;
  (*handle_msg)[E42_RIC_SUBSCRIPTION_DELETE_REQUEST] = e2ap_handle_e42_ric_subscription_delete_request_iapp;
  (*handle_msg)[E42_RIC_CONTROL_REQUEST] = e2ap_handle_e42_ric_control_request_iapp;
  (*handle_msg)[RIC_CONTROL_ACKNOWLEDGE] = e2ap_handle_e42_ric_control_ack_iapp;
  (*handle_msg)[RIC_INDICATION] = e2ap_handle_ric_indication_iapp;
  (*handle_msg)[RIC_SUBSCRIPTION_DELETE_RESPONSE] = e2ap_handle_subscription_delete_response_iapp;

  //  (*handle_msg)[RIC_SUBSCRIPTION_REQUEST] = e2ap_handle_subscription_request_iapp;
  //  (*handle_msg)[RIC_SUBSCRIPTION_DELETE_REQUEST] =  e2ap_handle_subscription_delete_request_iapp;
  //  (*handle_msg)[RIC_CONTROL_REQUEST] = e2ap_handle_control_request_iapp;
  //  (*handle_msg)[E2AP_ERROR_INDICATION] = e2ap_handle_error_indication_iapp;
  //  (*handle_msg)[E2_SETUP_REQUEST] = e2ap_handle_setup_request_iapp;
  //  (*handle_msg)[E2AP_RESET_REQUEST] =  e2ap_handle_reset_request_iapp;
  //  (*handle_msg)[E2AP_RESET_RESPONSE] =  e2ap_handle_reset_response_iapp;
  //  (*handle_msg)[RIC_SERVICE_UPDATE_ACKNOWLEDGE] =  e2ap_handle_service_update_ack_iapp;
  //  (*handle_msg)[RIC_SERVICE_UPDATE_FAILURE] =  e2ap_handle_service_update_failure_iapp;
  //  (*handle_msg)[RIC_SERVICE_QUERY] = e2ap_handle_service_query_iapp;
  //  (*handle_msg)[E2_NODE_CONFIGURATION_UPDATE_ACKNOWLEDGE] =  e2ap_handle_node_configuration_update_ack_iapp;
  //  (*handle_msg)[E2_NODE_CONFIGURATION_UPDATE_FAILURE] =  e2ap_handle_node_configuration_update_failure_iapp;
  //  (*handle_msg)[E2_CONNECTION_UPDATE] =  e2ap_handle_connection_update_iapp;
}

e2ap_msg_t e2ap_handle_subscription_response_iapp(e42_iapp_t* iapp, const e2ap_msg_t* msg)
{
  assert(iapp != NULL);
  assert(msg != NULL);
  assert(msg->type == RIC_SUBSCRIPTION_RESPONSE);

  ric_subscription_response_t const* src = &msg->u_msgs.ric_sub_resp;
  e2ap_msg_t none = {.type = NONE_E2_MSG_TYPE};

  // Find the xApp mapping with better error handling
  xapp_ric_id_xpct_t const xpctd = find_xapp_map_ric_id(&iapp->map_ric_id, src->ric_id.ric_req_id);
  if (!xpctd.has_value) {
    LOG_SURREY_RIC("[iApp]: ERROR - RIC Request ID %d not found in mapping\n", src->ric_id.ric_req_id);
    return none;
  }

  xapp_ric_id_t const x = xpctd.xapp_ric_id;

  // Verify RAN function ID and RIC instance ID match
  if (src->ric_id.ran_func_id != x.ric_id.ran_func_id) {
    LOG_SURREY_RIC("[iApp]: ERROR - RAN Function ID mismatch. Expected %d, got %d\n",
                   x.ric_id.ran_func_id,
                   src->ric_id.ran_func_id);
    return none;
  }

  if (src->ric_id.ric_inst_id != x.ric_id.ric_inst_id) {
    LOG_SURREY_RIC("[iApp]: ERROR - RIC Instance ID mismatch. Expected %d, got %d\n",
                   x.ric_id.ric_inst_id,
                   src->ric_id.ric_inst_id);
    return none;
  }

  // Create response message
  e2ap_msg_t ans = {.type = RIC_SUBSCRIPTION_RESPONSE};
  defer({ e2ap_msg_free_iapp(&iapp->ap, &ans); });

  // Copy and modify response
  ric_subscription_response_t* dst = &ans.u_msgs.ric_sub_resp;
  *dst = mv_ric_subscription_respponse(src);
  dst->ric_id.ric_req_id = x.ric_id.ric_req_id;

  // Prepare SCTP message
  sctp_msg_t sctp_msg = {0};
  sctp_msg.info = find_map_xapps_sad(&iapp->ep.xapps, x.xapp_id);

  // Verify SCTP association
  if (sctp_msg.info.addr.sin_port == 0) {
    LOG_SURREY_RIC("[iApp]: ERROR - Invalid SCTP association for xApp ID %d\n", x.xapp_id);
    return none;
  }

  // Encode and send message
  sctp_msg.ba = e2ap_msg_enc_iapp(&iapp->ap, &ans);
  defer({ free_sctp_msg(&sctp_msg); });

  e2ap_send_sctp_msg_iapp(&iapp->ep, &sctp_msg);

  return none;
}

e2ap_msg_t e2ap_handle_subscription_delete_response_iapp(e42_iapp_t* iapp, const e2ap_msg_t* msg)
{
  assert(iapp != NULL);
  assert(msg != NULL);
  assert(msg->type == RIC_SUBSCRIPTION_DELETE_RESPONSE);

  ric_subscription_delete_response_t const* src = &msg->u_msgs.ric_sub_del_resp;

  xapp_ric_id_xpct_t const xpctd = find_xapp_map_ric_id(&iapp->map_ric_id, src->ric_id.ric_req_id);
  if (xpctd.has_value == false) {
    printf("[iApp]: SUBSCRIPTION DELETE RESPONSE rx RAN_FUNC_ID %d RIC REQ ID %d but no xApp associated\n",
           src->ric_id.ran_func_id,
           src->ric_id.ric_req_id);
    e2ap_msg_t none = {.type = NONE_E2_MSG_TYPE};
    return none;
  }

  assert(xpctd.has_value == true && "RIC Req Id not found!");

  xapp_ric_id_t const x = xpctd.xapp_ric_id;

  assert(src->ric_id.ran_func_id == x.ric_id.ran_func_id);
  assert(src->ric_id.ric_inst_id == x.ric_id.ric_inst_id);

  e2ap_msg_t ans = {.type = RIC_SUBSCRIPTION_DELETE_RESPONSE};
  defer({ e2ap_msg_free_iapp(&iapp->ap, &ans); });
  ric_subscription_delete_response_t* dst = &ans.u_msgs.ric_sub_del_resp;
  dst->ric_id = x.ric_id;

  sctp_msg_t sctp_msg = {0};
  sctp_msg.info = find_map_xapps_sad(&iapp->ep.xapps, x.xapp_id);
  sctp_msg.ba = e2ap_msg_enc_iapp(&iapp->ap, &ans);
  defer({ free_sctp_msg(&sctp_msg); });

  e2ap_send_sctp_msg_iapp(&iapp->ep, &sctp_msg);

  printf("[iApp]: RIC_SUBSCRIPTION_DELETE_RESPONSE tx RAN_FUNC_ID %d RIC_REQ_ID %d \n", x.ric_id.ran_func_id, x.ric_id.ric_req_id);

  rm_map_ric_id(&iapp->map_ric_id, &x);

  e2ap_msg_t none = {.type = NONE_E2_MSG_TYPE};
  return none;
}

e2ap_msg_t e2ap_handle_e42_ric_control_ack_iapp(e42_iapp_t* iapp, const e2ap_msg_t* msg)
{
  assert(iapp != NULL);
  assert(msg != NULL);
  assert(msg->type == RIC_CONTROL_ACKNOWLEDGE);

  ric_control_acknowledge_t const* src = &msg->u_msgs.ric_ctrl_ack;

  xapp_ric_id_xpct_t const xpctd = find_xapp_map_ric_id(&iapp->map_ric_id, src->ric_id.ric_req_id);
  assert(xpctd.has_value == true && "RIC Req Id not found!");
  xapp_ric_id_t const x = xpctd.xapp_ric_id;

  assert(src->ric_id.ran_func_id == x.ric_id.ran_func_id);
  assert(src->ric_id.ric_inst_id == x.ric_id.ric_inst_id);

  e2ap_msg_t ans = {.type = RIC_CONTROL_ACKNOWLEDGE};
  defer({ e2ap_msg_free_iapp(&iapp->ap, &ans); });
  ric_control_acknowledge_t* dst = &ans.u_msgs.ric_ctrl_ack;
  dst->ric_id = x.ric_id;

#ifdef E2AP_V1
  dst->status = src->status;
#endif

  sctp_msg_t sctp_msg = {0};
  sctp_msg.info = find_map_xapps_sad(&iapp->ep.xapps, x.xapp_id);
  sctp_msg.ba = e2ap_msg_enc_iapp(&iapp->ap, &ans);
  defer({ free_sctp_msg(&sctp_msg); });

  e2ap_send_sctp_msg_iapp(&iapp->ep, &sctp_msg);

  printf("[iApp]: RIC_CONTROL_ACKNOWLEDGE tx\n");

  rm_map_ric_id(&iapp->map_ric_id, &x);

  e2ap_msg_t none = {.type = NONE_E2_MSG_TYPE};
  return none;
}

static e42_setup_response_t generate_setup_response(e42_iapp_t* iapp, e42_setup_request_t const* req)
{
  assert(iapp != NULL);
  assert(req != NULL);

  ran_function_t* rf = req->ran_func_item;
  assert(rf != NULL);
  size_t const len_rf = req->len_rf;
  assert(len_rf > 0);

  // ToDo, permissions and whether the SM exists in the iApp, should be checked
  e2_node_arr_t ans = generate_e2_node_arr(&iapp->e2_nodes);

  e42_setup_response_t sr = {.xapp_id = iapp->xapp_id++, .len_e2_nodes_conn = ans.len, .nodes = ans.n};

  return sr;
}

// xApp -> iApp
e2ap_msg_t e2ap_handle_e42_setup_request_iapp(e42_iapp_t* iapp, const e2ap_msg_t* msg)
{
  assert(iapp != NULL);
  assert(msg != NULL);
  assert(msg->type == E42_SETUP_REQUEST);
  const e42_setup_request_t* req = &msg->u_msgs.e42_stp_req;

  printf("[iApp]: E42 SETUP-REQUEST rx\n");

  e2ap_msg_t ans = {.type = E42_SETUP_RESPONSE};
  ans.u_msgs.e42_stp_resp = generate_setup_response(iapp, req);

  printf("[iApp]: E42 SETUP-RESPONSE tx\n");

  return ans;
}

static gtp_ind_msg_t decode_gtp_indication(size_t len, uint8_t const* buffer)
{
  gtp_ind_msg_t ret = {0};

  // Check minimum size requirement using next_pow2 trick
  assert(next_pow2(len) >= sizeof(gtp_ind_msg_t) - sizeof(gtp_ngu_t_stats_t*)
         && "Less bytes than the case where there are not active Radio bearers!");

  // First copy the length field
  memcpy(&ret.len, buffer, sizeof(ret.len));

  // Initialize iterator after length field
  void const* it = buffer + sizeof(ret.len);

  // Allocate memory for NGUT if needed
  if (ret.len > 0) {
    ret.ngut = calloc(ret.len, sizeof(gtp_ngu_t_stats_t));
    if (ret.ngut == NULL) {
      printf("Memory allocation failed for NGUT data\n");
      return ret;
    }

    // Copy NGUT data
    for (uint32_t i = 0; i < ret.len; ++i) {
      memcpy(&ret.ngut[i], it, sizeof(gtp_ngu_t_stats_t));
      it += sizeof(gtp_ngu_t_stats_t);
    }
  }

  // Copy timestamp
  memcpy(&ret.tstamp, it, sizeof(ret.tstamp));
  it += sizeof(ret.tstamp);

  // Copy Handover Info
  memcpy(&ret.ho_info, it, sizeof(ret.ho_info));
  it += sizeof(ret.ho_info);

  // Verify we've read exactly the right number of bytes
  assert(it == buffer + len && "Mismatch of data layout");

  return ret;
}

// Add a cleanup function
static void free_decoded_gtp_indication(gtp_ind_msg_t* msg)
{
  if (msg != NULL && msg->ngut != NULL) {
    free(msg->ngut);
    msg->ngut = NULL;
    msg->len = 0;
  }
}

gtp_ho_info_t extract_handover_info_ric(const uint8_t* buffer, size_t len)
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

// void print_ric_indication_content(const ric_indication_t* src)
// {
//   assert(src != NULL);

//   // printf("\n=== Indication Message Content ===\n");
//   // LOG_SURREY_RIC("Message Type: RIC_INDICATION\n");
//   // LOG_SURREY_RIC("RIC Request ID: %d\n", src->ric_id.ric_req_id);
//   // LOG_SURREY_RIC("RIC Instance ID: %d\n", src->ric_id.ric_inst_id);
//   // LOG_SURREY_RIC("RAN Function ID: %d\n", src->ric_id.ran_func_id);

//   // Print indication header if available
//   // if (src->hdr.buf != NULL) {
//   //   LOG_SURREY_RIC("Header Content (%zu bytes):\n", src->hdr.len);
//   //   printf("Header: \n");
//   //   for (size_t i = 0; i < src->hdr.len; i++) {
//   //     printf("%02X ", src->hdr.buf[i]);
//   //     if ((i + 1) % 16 == 0)
//   //       printf("\n     ");
//   //   }
//   //   printf("\n");
//   // }

//   // Print indication message if available
//   if (src->msg.buf != NULL) {
//     // Get raw bytes
//     uint8_t* raw_bytes = (uint8_t*)src->msg.buf;

//     // Extract handover info
//     gtp_ho_info_t ho_info = extract_handover_info_ric(raw_bytes, src->msg.len);

//     if (ho_info.ho_complete == true) {
//       // Print raw bytes LOG_SURREY_RIC("Raw bytes:\n");
//       for (size_t i = 0; i < src->msg.len; i++) {
//         printf("%02x ", raw_bytes[i]);
//         if ((i + 1) % 16 == 0)
//           printf("\n");
//       }
//       printf("\n");

//       // Print the results
//       LOG_SURREY_RIC("Extracted Handover Info:\n");
//       LOG_SURREY_RIC("UE %u (Source DU: %u, Target DU: %u, HO Complete: %s)\n",
//                      ho_info.ue_id,
//                      ho_info.source_du,
//                      ho_info.target_du,
//                      ho_info.ho_complete ? "true" : "false");
//     }
//     // printf("\n===End indication Message Content===\n");
//   }
// }

// void decode_gtp_indication_message(ric_indication_t const* src)
// {
//   LOG_SURREY_RIC("\n=== Decoding GTP Indication at RIC ===\n");

//   if (src == NULL || src->msg.buf == NULL) {
//     LOG_SURREY_RIC("Invalid indication message or SM payload\n");
//     return;
//   }

//   // Decode the message
//   gtp_ind_msg_t decoded = decode_gtp_indication(src->msg.len, (uint8_t const*)src->msg.buf);

//   // Print decoded information
//   LOG_SURREY_RIC("\nDecoded GTP Message Content:\n");
//   LOG_SURREY_RIC("Length: %u\n", decoded.len);

//   // Print handover information
//   LOG_SURREY_RIC("Handover Information:\n");
//   LOG_SURREY_RIC("  UE ID: %u\n", decoded.ho_info.ue_id);
//   LOG_SURREY_RIC("  Source DU: %u\n", decoded.ho_info.source_du);
//   LOG_SURREY_RIC("  Target DU: %u\n", decoded.ho_info.target_du);
//   LOG_SURREY_RIC("  HO Complete: %s\n", decoded.ho_info.ho_complete ? "Yes" : "No");

//   LOG_SURREY_RIC("Timestamp: %lu\n", decoded.tstamp);
//   LOG_SURREY_RIC("===============================\n\n");

//   // Clean up
//   free_decoded_gtp_indication(&decoded);
// }

static void forward_indication_to_xapp(e42_iapp_t* iapp, uint32_t xapp_id, const ric_indication_t* ind)
{
  assert(iapp != NULL);
  assert(ind != NULL);
  // Lock using the structure mutex
  pthread_mutex_lock(&iapp->forward_mutex);

  // LOG_SURREY_RIC("[iApp]: Starting indication forward to xApp %d\n", xapp_id);

  e2ap_msg_t ans = {.type = RIC_INDICATION};
  // defer({ e2ap_msg_free_iapp(&iapp->ap, &ans); });

  ric_indication_t* dst = &ans.u_msgs.ric_ind;

  // Create a copy of the indication
  ric_indication_t temp_ind = {0};
  memcpy(&temp_ind, ind, sizeof(ric_indication_t));

  // Copy buffer contents if present
  if (ind->hdr.buf != NULL) {
    temp_ind.hdr.buf = malloc(ind->hdr.len);
    assert(temp_ind.hdr.buf != NULL);
    memcpy(temp_ind.hdr.buf, ind->hdr.buf, ind->hdr.len);
    temp_ind.hdr.len = ind->hdr.len;
  }

  if (ind->msg.buf != NULL) {
    temp_ind.msg.buf = malloc(ind->msg.len);
    assert(temp_ind.msg.buf != NULL);
    memcpy(temp_ind.msg.buf, ind->msg.buf, ind->msg.len);
    temp_ind.msg.len = ind->msg.len;
  }

  *dst = mv_ric_indication(&temp_ind);

  sctp_msg_t sctp_msg = {0};
  sctp_msg.info = find_map_xapps_sad(&iapp->ep.xapps, xapp_id);

  if (sctp_msg.info.addr.sin_port == 0) {
    LOG_SURREY_RIC("[iApp]: ERROR - xApp %d not connected for indication forwarding\n", xapp_id);
    return;
  }

  // LOG_SURREY_RIC("[iApp]: Encoding indication message for xApp %d\n", xapp_id);
  sctp_msg.ba = e2ap_msg_enc_iapp(&iapp->ap, &ans);

  if (sctp_msg.ba.buf == NULL || sctp_msg.ba.len == 0) {
    LOG_SURREY_RIC("[iApp]: ERROR - Failed to encode indication message\n");
    return;
  }
  e2ap_send_sctp_msg_iapp(&iapp->ep, &sctp_msg);
  // defer({ free_sctp_msg(&sctp_msg); });

  // LOG_SURREY_RIC("[iApp]: Sending indication to xApp %d (message size: %zu bytes)\n", xapp_id, sctp_msg.ba.len);
  // Cleanup
  free_sctp_msg(&sctp_msg);
  e2ap_msg_free_iapp(&iapp->ap, &ans);
  // Unlock using the structure mutex
  pthread_mutex_unlock(&iapp->forward_mutex);
}

e2ap_msg_t e2ap_handle_ric_indication_iapp(e42_iapp_t* iapp, const e2ap_msg_t* msg)
{
  assert(iapp != NULL);
  assert(msg != NULL);
  assert(msg->type == RIC_INDICATION);

  ric_indication_t const* src = &msg->u_msgs.ric_ind;

  // LOG_SURREY_RIC("[iApp]: Received RIC Indication for RAN Function ID: %d\n", src->ric_id.ran_func_id);

  // Debug print subscription registry state
  pthread_mutex_lock(&iapp->subscription_registry.mutex);
  // LOG_SURREY_RIC("[iApp]: Current Subscription Registry State:\n");
  // LOG_SURREY_RIC("        Total subscriptions: %zu\n", iapp->subscription_registry.count);

  bool indication_forwarded = false;

  // Check all active subscriptions
  for (size_t i = 0; i < iapp->subscription_registry.count; i++) {
    subscription_entry_t* entry = &iapp->subscription_registry.entries[i];

    // LOG_SURREY_RIC(
    //     "[iApp]: Checking subscription entry %zu:\n"
    //     "        xApp ID: %d\n"
    //     "        RAN Function ID: %d (Received: %d)\n"
    //     "        Active: %s\n",
    //     i,
    //     entry->xapp_id,
    //     entry->ran_func_id,
    //     src->ric_id.ran_func_id,
    //     entry->active ? "true" : "false");

    if (entry->active && entry->ran_func_id == src->ric_id.ran_func_id) {
      // LOG_SURREY_RIC(
      //     "[iApp]: Found matching subscription - forwarding indication\n"
      //     "        From RAN func %d to xApp %d\n",
      //     src->ric_id.ran_func_id,
      //     entry->xapp_id);

      // Forward indication to subscribed xApp
      forward_indication_to_xapp(iapp, entry->xapp_id, src);
      indication_forwarded = true;
    }
  }

  pthread_mutex_unlock(&iapp->subscription_registry.mutex);

  if (!indication_forwarded) {
    LOG_SURREY_RIC("[iApp]: No active subscriptions found for RAN function %d\n", src->ric_id.ran_func_id);
  }

  e2ap_msg_t none = {.type = NONE_E2_MSG_TYPE};
  return none;
}

static bool valid_xapp_id(e42_iapp_t* iapp, uint32_t xapp_id)
{
  assert(iapp != NULL);
  assert(xapp_id < 1 << 16);
  return xapp_id <= iapp->xapp_id;
}

static bool valid_global_e2_node(e42_iapp_t* iapp, global_e2_node_id_t const* id)
{
  assert(iapp != NULL);
  assert(id != NULL);

  e2_node_arr_t nodes = generate_e2_node_arr(&iapp->e2_nodes);
  // defer( { free_e2_node_arr(&nodes);  }  );

  for (size_t i = 0; i < nodes.len; ++i) {
    if (eq_global_e2_node_id(&nodes.n[i].id, id) == true) {
      free_e2_node_arr(&nodes);
      return true;
    }
  }

  free_e2_node_arr(&nodes);
  return false;
}

e2ap_msg_t e2ap_handle_e42_ric_subscription_delete_request_iapp(e42_iapp_t* iapp, const e2ap_msg_t* msg)
{
  assert(iapp != NULL);
  assert(msg != NULL);
  assert(msg->type == E42_RIC_SUBSCRIPTION_DELETE_REQUEST);

  const e42_ric_subscription_delete_request_t* src = &msg->u_msgs.e42_ric_sub_del_req;

  xapp_ric_id_t x = {.ric_id = src->sdr.ric_id, .xapp_id = src->xapp_id};

  e2_node_ric_id_t n = find_ric_req_map_ric_id(&iapp->map_ric_id, &x);
  assert(n.ric_req_type == SUBSCRIPTION_RIC_REQUEST_TYPE);

  ric_subscription_delete_request_t dst = cp_ric_subscription_delete_request(&src->sdr);
  dst.ric_id.ric_req_id = n.ric_id.ric_req_id;

  fwd_ric_subscription_request_delete_gen(iapp->ric_if.type, &n.e2_node_id, &dst, notify_msg_iapp_api);

  printf("[iApp]: RIC_SUBSCRIPTION_DELETE_REQUEST tx RIC_REQ_ID %d \n", n.ric_id.ric_req_id);

  e2ap_msg_t ans = {.type = NONE_E2_MSG_TYPE};
  return ans;
}

// xApp -> iApp
e2ap_msg_t e2ap_handle_e42_ric_subscription_request_iapp(e42_iapp_t* iapp, const e2ap_msg_t* msg)
{
  assert(iapp != NULL);
  assert(msg != NULL);
  assert(msg->type == E42_RIC_SUBSCRIPTION_REQUEST);

  e42_ric_subscription_request_t const* e42_sr = &msg->u_msgs.e42_ric_sub_req;
  e2ap_msg_t none = {.type = NONE_E2_MSG_TYPE};

  // LOG_SURREY_RIC(
  //     "[iApp]: Starting subscription request processing:\n"
  //     "        xApp ID: %d\n"
  //     "        RAN Function ID: %d\n"
  //     "        Current registry count: %zu\n",
  //     e42_sr->xapp_id,
  //     e42_sr->sr.ric_id.ran_func_id,
  //     iapp->subscription_registry.count);

  // Validate inputs
  if (!valid_xapp_id(iapp, e42_sr->xapp_id)) {
    LOG_SURREY_RIC("[iApp]: ERROR - Invalid xApp ID %d\n", e42_sr->xapp_id);
    return none;
  }

  // Check xApp connection
  sctp_msg_t check_msg = {0};
  check_msg.info = find_map_xapps_sad(&iapp->ep.xapps, e42_sr->xapp_id);
  if (check_msg.info.addr.sin_port == 0) {
    LOG_SURREY_RIC("[iApp]: ERROR - xApp %d not connected\n", e42_sr->xapp_id);
    return none;
  }

  // Generate new RIC request ID
  uint16_t new_ric_id = fwd_ric_subscription_request_gen(iapp->ric_if.type, &e42_sr->id, &e42_sr->sr, notify_msg_iapp_api);

  if (new_ric_id == 0) {
    LOG_SURREY_RIC("[iApp]: ERROR - Failed to generate RIC request ID\n");
    return none;
  }

  // LOG_SURREY_RIC("[iApp]: Generated new RIC request ID: %d\n", new_ric_id);

  // Debug print current state
  // LOG_SURREY_RIC(
  //     "[iApp]: Current registry state before adding:\n"
  //     "        Capacity: %zu\n"
  //     "        Count: %zu\n",
  //     iapp->subscription_registry.capacity,
  //     iapp->subscription_registry.count);

  // Check and expand capacity if needed
  // if (iapp->subscription_registry.count >= iapp->subscription_registry.capacity) {
  //   size_t new_capacity = (iapp->subscription_registry.capacity == 0) ? 32 : iapp->subscription_registry.capacity * 2;
  //   LOG_SURREY_RIC("[iApp]: Expanding registry capacity from %zu to %zu\n", iapp->subscription_registry.capacity, new_capacity);

  //   subscription_entry_t* new_entries = realloc(iapp->subscription_registry.entries, new_capacity *
  //   sizeof(subscription_entry_t)); if (!new_entries) {
  //     LOG_SURREY_RIC("[iApp]: ERROR - Failed to allocate memory for registry expansion\n");
  //     pthread_mutex_unlock(&iapp->subscription_registry.mutex);
  //     return none;
  //   }
  //   iapp->subscription_registry.entries = new_entries;
  //   iapp->subscription_registry.capacity = new_capacity;
  // }
  // Replace the manual subscription handling with register_subscription
  bool reg_success =
      register_subscription(&iapp->subscription_registry, e42_sr->xapp_id, e42_sr->sr.ric_id.ran_func_id, new_ric_id);

  if (!reg_success) {
    LOG_SURREY_RIC("[iApp]: ERROR - Failed to register subscription\n");
    return none;
  }
  // // Add new subscription
  // size_t idx = iapp->subscription_registry.count;
  // iapp->subscription_registry.entries[idx].xapp_id = e42_sr->xapp_id;
  // iapp->subscription_registry.entries[idx].ran_func_id = e42_sr->sr.ric_id.ran_func_id;
  // iapp->subscription_registry.entries[idx].ric_req_id = new_ric_id;
  // iapp->subscription_registry.entries[idx].active = true;
  // iapp->subscription_registry.count++;

  // LOG_SURREY_RIC(
  //     "[iApp]: Added new subscription:\n"
  //     "        xApp ID: %d\n"
  //     "        RAN Function ID: %d\n"
  //     "        RIC Request ID: %d\n"
  //     "        New registry count: %zu\n",
  //     e42_sr->xapp_id,
  //     e42_sr->sr.ric_id.ran_func_id,
  //     new_ric_id,
  //     iapp->subscription_registry.count);

  // Verify the entry was added correctly
  // bool verify_ok = false;
  // for (size_t i = 0; i < iapp->subscription_registry.count; i++) {
  //   if (iapp->subscription_registry.entries[i].xapp_id == e42_sr->xapp_id
  //       && iapp->subscription_registry.entries[i].ran_func_id == e42_sr->sr.ric_id.ran_func_id
  //       && iapp->subscription_registry.entries[i].active) {
  //     verify_ok = true;
  //     break;
  //   }
  // }

  // if (!verify_ok) {
  //   LOG_SURREY_RIC("[iApp]: ERROR - Failed to verify subscription addition\n");
  //   return none;
  // }

  // LOG_SURREY_RIC("[iApp]: Successfully registered subscription\n");

  // Add to RIC ID map
  pthread_rwlock_wrlock(&iapp->map_ric_id.rw);

  e2_node_ric_id_t node = {.ric_id = e42_sr->sr.ric_id,
                           .e2_node_id = cp_global_e2_node_id(&e42_sr->id),
                           .ric_req_type = SUBSCRIPTION_RIC_REQUEST_TYPE};
  node.ric_id.ric_req_id = new_ric_id;

  xapp_ric_id_t xapp_ric_id = {.ric_id = e42_sr->sr.ric_id, .xapp_id = e42_sr->xapp_id};
  xapp_ric_id.ric_id.ric_req_id = new_ric_id;

  bi_map_insert(&iapp->map_ric_id.bimap, &node, sizeof(node), &xapp_ric_id, sizeof(xapp_ric_id));
  pthread_rwlock_unlock(&iapp->map_ric_id.rw);

  return none;
}

// xApp -> iApp
e2ap_msg_t e2ap_handle_e42_ric_control_request_iapp(e42_iapp_t* iapp, const e2ap_msg_t* msg)
{
  assert(iapp != NULL);
  assert(msg != NULL);
  assert(msg->type == E42_RIC_CONTROL_REQUEST);

  e42_ric_control_request_t const* e42_cr = &msg->u_msgs.e42_ric_ctrl_req;

  assert(valid_xapp_id(iapp, e42_cr->xapp_id) == true);
  assert(valid_global_e2_node(iapp, &e42_cr->id));

  xapp_ric_id_t xapp_ric_id = {.ric_id = e42_cr->ctrl_req.ric_id, .xapp_id = e42_cr->xapp_id};

  // I do not like the mtx here but there is a data race if not
  int rc = pthread_rwlock_wrlock(&iapp->map_ric_id.rw);
  assert(rc == 0);

  uint16_t new_ric_id = fwd_ric_control_request_gen(iapp->ric_if.type, &e42_cr->id, &e42_cr->ctrl_req, notify_msg_iapp_api);

  e2_node_ric_id_t n = {.ric_id = e42_cr->ctrl_req.ric_id, //  new_ric_id,
                        .e2_node_id = cp_global_e2_node_id(&e42_cr->id),
                        .ric_req_type = CONTROL_RIC_REQUEST_TYPE};
  n.ric_id.ric_req_id = new_ric_id;

  add_map_ric_id(&iapp->map_ric_id, &n, &xapp_ric_id);
  rc = pthread_rwlock_unlock(&iapp->map_ric_id.rw);
  assert(rc == 0);

  printf("[iApp]: E42_RIC_CONTROL_REQUEST rx\n");

  e2ap_msg_t ans = {.type = NONE_E2_MSG_TYPE};
  return ans;
}

e2ap_msg_t e2ap_msg_handle_iapp(e42_iapp_t* iapp, const e2ap_msg_t* msg)
{
  assert(msg != NULL);
  const e2_msg_type_t msg_type = msg->type;
  assert(check_valid_msg_type(msg_type) == true);
  assert(iapp->handle_msg[msg_type] != NULL);
  return iapp->handle_msg[msg_type](iapp, msg);
}

e2ap_msg_t e2ap_handle_subscription_delete_request_iapp(e42_iapp_t* iapp, const e2ap_msg_t* msg)
{
  assert(iapp != NULL);
  assert(msg != NULL);
  assert(msg->type == RIC_SUBSCRIPTION_DELETE_REQUEST);

  assert(0 != 0 && "Should not come here");

  e2ap_msg_t ans = {.type = NONE_E2_MSG_TYPE};
  return ans;
}

// The purpose of the RIC Control procedure is to initiate or resume a specific functionality in the E2 Node.
e2ap_msg_t e2ap_handle_control_request_iapp(e42_iapp_t* iapp, const e2ap_msg_t* msg)
{
  assert(iapp != NULL);
  assert(msg != NULL);
  assert(msg->type == RIC_CONTROL_REQUEST);

  assert(0 != 0 && "Should not come here...");

  printf("[E2-AGENT]: CONTROL ACKNOWLEDGE tx\n");
  e2ap_msg_t ans = {.type = RIC_CONTROL_ACKNOWLEDGE};

  return ans;
}

e2ap_msg_t e2ap_handle_error_indication_iapp(e42_iapp_t* iapp, const e2ap_msg_t* msg)
{
  assert(iapp != NULL);
  assert(msg != NULL);
  assert(0 != 0 && "Not implemented");

  e2ap_msg_t ans = {.type = NONE_E2_MSG_TYPE};
  return ans;
}

e2ap_msg_t e2ap_handle_reset_request_iapp(e42_iapp_t* iapp, const e2ap_msg_t* msg)
{
  assert(iapp != NULL);
  assert(msg != NULL);
  assert(0 != 0 && "Not implemented");

  e2ap_msg_t ans = {.type = NONE_E2_MSG_TYPE};
  return ans;
}

e2ap_msg_t e2ap_handle_reset_response_iapp(e42_iapp_t* iapp, const e2ap_msg_t* msg)
{
  assert(iapp != NULL);
  assert(msg != NULL);
  assert(0 != 0 && "Not implemented");

  e2ap_msg_t ans = {.type = NONE_E2_MSG_TYPE};
  return ans;
}

e2ap_msg_t e2ap_handle_service_update_ack_iapp(e42_iapp_t* iapp, const e2ap_msg_t* msg)
{
  assert(iapp != NULL);
  assert(msg != NULL);
  assert(0 != 0 && "Not implemented");

  e2ap_msg_t ans = {.type = NONE_E2_MSG_TYPE};
  return ans;
}

e2ap_msg_t e2ap_handle_service_update_failure_iapp(e42_iapp_t* iapp, const e2ap_msg_t* msg)
{
  assert(iapp != NULL);
  assert(msg != NULL);
  assert(0 != 0 && "Not implemented");

  e2ap_msg_t ans = {.type = NONE_E2_MSG_TYPE};
  return ans;
}

e2ap_msg_t e2ap_handle_service_query_iapp(e42_iapp_t* iapp, const e2ap_msg_t* msg)
{
  assert(iapp != NULL);
  assert(msg != NULL);
  assert(0 != 0 && "Not implemented");

  e2ap_msg_t ans = {.type = NONE_E2_MSG_TYPE};
  return ans;
}

e2ap_msg_t e2ap_handle_node_configuration_update_ack_iapp(e42_iapp_t* iapp, const e2ap_msg_t* msg)
{
  assert(iapp != NULL);
  assert(msg != NULL);
  assert(0 != 0 && "Not implemented");

  e2ap_msg_t ans = {.type = NONE_E2_MSG_TYPE};
  return ans;
}

e2ap_msg_t e2ap_handle_node_configuration_update_failure_iapp(e42_iapp_t* iapp, const e2ap_msg_t* msg)
{
  assert(iapp != NULL);
  assert(msg != NULL);
  assert(0 != 0 && "Not implemented");

  e2ap_msg_t ans = {.type = NONE_E2_MSG_TYPE};
  return ans;
}

e2ap_msg_t e2ap_handle_connection_update_iapp(e42_iapp_t* iapp, const e2ap_msg_t* msg)
{
  assert(iapp != NULL);
  assert(msg != NULL);
  assert(0 != 0 && "Not implemented");

  e2ap_msg_t ans = {.type = NONE_E2_MSG_TYPE};
  return ans;
}
