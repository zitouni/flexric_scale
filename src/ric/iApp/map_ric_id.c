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

#include "../../util/alg_ds/alg/alg.h"
#include "../../util/alg_ds/ds/lock_guard/lock_guard.h"
#include "map_ric_id.h"
#include "xapp_ric_id.h"

#include "../../../../RAN_FUNCTION/surrey_log.h"

#include <assert.h>

#include <time.h>
#include <string.h>

static inline int cmp_uint32(const void* m0_v, const void* m1_v)
{
  uint32_t* m0 = (uint32_t*)m0_v;
  uint32_t* m1 = (uint32_t*)m1_v;
  if (*m0 < *m1)
    return -1;
  else if (*m0 == *m1)
    return 0;

  return 1;
}

static void free_xapp_ric_gen_id(void* key, void* value)
{
  assert(key != NULL);
  assert(value != NULL);

  (void)key;

  xapp_ric_id_t* id = (xapp_ric_id_t*)value;
  free(id);
}

static void free_e2_node_ric_req(void* key, void* value)
{
  assert(key != NULL);
  assert(value != NULL);

  (void)key;

  e2_node_ric_id_t* n = (e2_node_ric_id_t*)value;

  free_e2_node_ric_id(n);
  free(n);
}

static inline int cmp_e2_node_ric_req_wrapper(void const* m0_v, void const* m1_v)
{
  assert(m0_v != NULL);
  assert(m1_v != NULL);

  e2_node_ric_id_t* m0 = (e2_node_ric_id_t*)m0_v;
  e2_node_ric_id_t* m1 = (e2_node_ric_id_t*)m1_v;

  return cmp_uint32(&m0->ric_id.ric_req_id, &m1->ric_id.ric_req_id);

  // return cmp_ric_gen_id(&m0->ric_id, &m1->ric_id);
}

void init_map_ric_id(map_ric_id_t* map)
{
  assert(map != NULL);

  pthread_rwlockattr_t attr = {0};
  int rc = pthread_rwlock_init(&map->rw, &attr);
  assert(rc == 0);

  size_t key_sz_1 = sizeof(e2_node_ric_id_t);
  size_t key_sz_2 = sizeof(xapp_ric_id_t); //);

  bi_map_init(&map->bimap,
              key_sz_1,
              key_sz_2,
              cmp_e2_node_ric_req_wrapper,
              cmp_xapp_ric_gen_id_wrapper,
              free_xapp_ric_gen_id,
              free_e2_node_ric_req);
}

void free_map_ric_id(map_ric_id_t* map)
{
  assert(map != NULL);

  // int rc = pthread_mutex_destroy(&map->mtx);
  // assert(rc == 0);

  int const rc = pthread_rwlock_destroy(&map->rw);
  assert(rc == 0);

  bi_map_free(&map->bimap);
  //  assoc_free(&map->tree);
}

/*
static inline
bool eq_uint16(void const* m0, void const* m1)
{
  return *(uint16_t*)m0 == *(uint16_t*)m1;
}
*/

void add_map_ric_id(map_ric_id_t* map, e2_node_ric_id_t* node, xapp_ric_id_t* xapp)
{
  assert(map != NULL);
  assert(node != NULL);
  assert(xapp != NULL);

  // Simple lock without timeout
  int rc = pthread_rwlock_wrlock(&map->rw);
  if (rc != 0) {
    LOG_SURREY_RIC("[iApp]: ERROR - Failed to acquire write lock in add_map_ric_id (rc=%d)\n", rc);
    return;
  }

  // First try to remove any existing mapping for this node
  assoc_rb_tree_t* left = &map->bimap.left;
  void* it = assoc_front(left);
  void* end = assoc_end(left);

  it = find_if(left, it, end, node, eq_e2_node_ric_req);
  if (it != end) {
    // Remove existing mapping using bi_map_extract_left
    void (*free_fn)(void*) = NULL; // We don't want to free the value
    bi_map_extract_left(&map->bimap, node, sizeof(*node), free_fn);
  }

  // Add new mapping
  bi_map_insert(&map->bimap, node, sizeof(*node), xapp, sizeof(*xapp));
  LOG_SURREY_RIC("[iApp]: Added mapping for RIC Request ID %d\n", node->ric_id.ric_req_id);

  rc = pthread_rwlock_unlock(&map->rw);
  if (rc != 0) {
    LOG_SURREY_RIC("[iApp]: ERROR - Failed to release write lock (rc=%d)\n", rc);
  }
}

void rm_map_ric_id(map_ric_id_t* map, xapp_ric_id_t const* ric_id)
{
  assert(map != NULL);
  assert(ric_id != NULL);

  int rc = pthread_rwlock_wrlock(&map->rw);
  assert(rc == 0);

  // left: key1:   e2_node_ric_id_t | value: xapp_ric_id_t
  // right: key2:  xapp_ric_id_t | value: e2_node_ric_id_t

  // It returns the void* of key1. the void* of the key2 is freed
  void (*free_xapp_ric_id)(void*) = NULL;
  e2_node_ric_id_t* n =
      (e2_node_ric_id_t*)bi_map_extract_right(&map->bimap, (void*)ric_id, sizeof(xapp_ric_id_t), free_xapp_ric_id);

  // printf("Removing xapp_ric_id xapp %d ric_req_id %d node ric id %d \n", ric_id->xapp_id, ric_id->ric_id.ric_req_id,
  // n->ric_id.ric_req_id);

  free_e2_node_ric_id(n);
  free(n);

  rc = pthread_rwlock_unlock(&map->rw);
  assert(rc == 0);
}

#include "map_ric_id.h"
#include "../../util/alg_ds/alg/alg.h"
#include "../../util/alg_ds/ds/lock_guard/lock_guard.h"
#include "xapp_ric_id.h"
#include "../../../../RAN_FUNCTION/surrey_log.h"
#include <assert.h>
#include <time.h>
#include <string.h>

// For nanosleep
#include <time.h>

xapp_ric_id_xpct_t find_xapp_map_ric_id(map_ric_id_t* map, uint16_t ric_req_id)
{
  assert(map != NULL);

  xapp_ric_id_xpct_t ans = {.has_value = false};

  // Create dummy node for search
  e2_node_ric_id_t dummy_node = {.ric_id.ric_req_id = ric_req_id, .ric_req_type = SUBSCRIPTION_RIC_REQUEST_TYPE};

  // Try multiple times to acquire the lock
  int max_retries = 3;
  int retry_count = 0;
  int rc;

  do {
    rc = pthread_rwlock_rdlock(&map->rw);
    if (rc == 0) {
      break;
    }
    retry_count++;
    if (retry_count < max_retries) {
      // Small delay before retry
      struct timespec ts = {0, 100000000}; // 100ms
      nanosleep(&ts, NULL);
    }
  } while (retry_count < max_retries);
  if (rc != 0) {
    LOG_SURREY_RIC("[iApp]: ERROR - Failed to acquire read lock (rc=%d)\n", rc);
    return ans;
  }

  assoc_rb_tree_t* left = &map->bimap.left;
  void* it = assoc_front(left);
  void* end = assoc_end(left);

  it = find_if(left, it, end, &dummy_node, eq_e2_node_ric_req);

  if (it != end) {
    void* value = assoc_value(left, it);
    if (value != NULL) {
      ans.has_value = true;
      memcpy(&ans.xapp_ric_id, value, sizeof(xapp_ric_id_t));
      // LOG_SURREY_RIC("[iApp]: Found mapping for RIC Request ID %d\n", ric_req_id);
    }
  } else {
    LOG_SURREY_RIC("[iApp]: No mapping found for RIC Request ID %d\n", ric_req_id);
  }

  pthread_rwlock_unlock(&map->rw);
  return ans;
}

e2_node_ric_id_t find_ric_req_map_ric_id(map_ric_id_t* map, xapp_ric_id_t* x)
{
  assert(map != NULL);

  int rc = pthread_rwlock_rdlock(&map->rw);
  assert(rc == 0);

  assoc_rb_tree_t* r = &map->bimap.right;

  void* it = assoc_front(r);
  void* end = assoc_end(r);
  it = find_if(r, it, end, x, eq_xapp_ric_gen_id_wrapper);
  assert(it != end && "Not found xApp RIC ID ");

  e2_node_ric_id_t const id = *(e2_node_ric_id_t*)assoc_value(r, it);

  rc = pthread_rwlock_unlock(&map->rw);
  assert(rc == 0);

  return id;
}

// array of e2_node_ric_id_t
seq_arr_t find_all_subs_map_ric_id(map_ric_id_t* map, uint16_t xapp_id)
{
  assert(map != NULL);

  seq_arr_t arr = {0};
  seq_init(&arr, sizeof(e2_node_ric_id_t));

  xapp_ric_id_t first_xapp_id = {// ric_gen_id_t .ric_id = ,
                                 .xapp_id = xapp_id};

  int rc = pthread_rwlock_rdlock(&map->rw);
  assert(rc == 0);

  assoc_rb_tree_t* right = &map->bimap.right;

  void* it = assoc_front(right);
  void* end = assoc_end(right);
  it = find_if(right, it, end, &first_xapp_id, eq_xapp_id_gen_wrapper);

  while (it != end) {
    e2_node_ric_id_t* n = (e2_node_ric_id_t*)assoc_value(right, it);

    if (n->ric_req_type == SUBSCRIPTION_RIC_REQUEST_TYPE) {
      e2_node_ric_id_t tmp = cp_e2_node_ric_id(n);
      seq_arr_push_back(&arr, &tmp, sizeof(e2_node_ric_id_t));
    }

    it = find_if(right, assoc_next(right, it), end, &first_xapp_id, eq_xapp_id_gen_wrapper);
  }

  rc = pthread_rwlock_unlock(&map->rw);
  assert(rc == 0);

  return arr;
}
