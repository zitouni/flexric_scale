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

#ifndef MAP_RIC_ID_H
#define MAP_RIC_ID_H

#include "../../util/alg_ds/ds/assoc_container/assoc_generic.h"

#include "e2_node_ric_id.h"

#include "xapp_ric_id.h"
#include <pthread.h>

typedef struct {
  //  assoc_rb_tree_t tree; // key: ric_req_id | value:   xapp_ric_id_t

  bi_map_t bimap; // left: key:   e2_node_ric_req_t | value: xapp_ric_id_t
                  // right: key:  xapp_ric_id_t | value: e2_node_ric_req_t
  pthread_rwlock_t rw;
} map_ric_id_t;

void init_map_ric_id(map_ric_id_t* map);

void free_map_ric_id(map_ric_id_t* map);

void add_map_ric_id(map_ric_id_t* map, e2_node_ric_id_t* node, xapp_ric_id_t* x);

void rm_map_ric_id(map_ric_id_t* map, xapp_ric_id_t const* ric_id);

// void rm_map_ric_id(map_ric_id_t* map, e2_node_ric_req_t* node); // uint16_t ric_req_id);

xapp_ric_id_xpct_t find_xapp_map_ric_id(map_ric_id_t* map, uint16_t ric_req_id);

e2_node_ric_id_t find_ric_req_map_ric_id(map_ric_id_t* map, xapp_ric_id_t* x);

// array of e2_node_ric_id_t
seq_arr_t find_all_subs_map_ric_id(map_ric_id_t* map, uint16_t xapp_id);

static inline bool eq_e2_node_ric_req(void const* m0_v, void const* m1_v)
{
  assert(m0_v != NULL);
  assert(m1_v != NULL);

  e2_node_ric_id_t* m0 = (e2_node_ric_id_t*)m0_v;
  e2_node_ric_id_t* m1 = (e2_node_ric_id_t*)m1_v;

  // return eq_ric_gen_id(&m0->ric_id, &m1->ric_id);
  return m0->ric_id.ric_req_id == m1->ric_id.ric_req_id;
}

#endif
