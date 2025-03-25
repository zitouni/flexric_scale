/*
 * File: subscription_registry.c
 * Author: Rafik ZITOUNI
 * Date: 2025-03-23
 *
 * License: MIT License
 *
 * Copyright (c) 2025 Rafik ZITOUNI
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * ...
 */

#include "subscription_registry.h"
#include <stdlib.h>
#include <string.h>

void init_subscription_registry(subscription_registry_t* registry)
{
  registry->capacity = 32;
  registry->count = 0;
  registry->entries = calloc(registry->capacity, sizeof(subscription_entry_t));
  pthread_mutex_init(&registry->mutex, NULL);
}

void free_subscription_registry(subscription_registry_t* registry)
{
  pthread_mutex_lock(&registry->mutex);
  free(registry->entries);
  registry->entries = NULL;
  registry->count = 0;
  registry->capacity = 0;
  pthread_mutex_unlock(&registry->mutex);
  pthread_mutex_destroy(&registry->mutex);
}

// Add or update subscription
bool register_subscription(subscription_registry_t* registry, uint32_t xapp_id, uint16_t ran_func_id, uint16_t ric_req_id)
{
  pthread_mutex_lock(&registry->mutex);

  // Check if subscription already exists for this xApp and RAN function
  for (size_t i = 0; i < registry->count; i++) {
    if (registry->entries[i].xapp_id == xapp_id && registry->entries[i].ran_func_id == ran_func_id) {
      // Update existing subscription
      registry->entries[i].ric_req_id = ric_req_id;
      registry->entries[i].active = true;
      pthread_mutex_unlock(&registry->mutex);
      return true;
    }
  }

  // Add new subscription
  if (registry->count >= registry->capacity) {
    size_t new_capacity = registry->capacity * 2;
    subscription_entry_t* new_entries = realloc(registry->entries, new_capacity * sizeof(subscription_entry_t));
    if (!new_entries) {
      pthread_mutex_unlock(&registry->mutex);
      return false;
    }
    registry->entries = new_entries;
    registry->capacity = new_capacity;
  }

  registry->entries[registry->count] =
      (subscription_entry_t){.xapp_id = xapp_id, .ran_func_id = ran_func_id, .ric_req_id = ric_req_id, .active = true};
  registry->count++;

  pthread_mutex_unlock(&registry->mutex);
  return true;
}

bool deactivate_subscription(subscription_registry_t* registry, uint32_t xapp_id, uint16_t ran_func_id)
{
  pthread_mutex_lock(&registry->mutex);
  bool found = false;

  for (size_t i = 0; i < registry->count; i++) {
    if (registry->entries[i].xapp_id == xapp_id && registry->entries[i].ran_func_id == ran_func_id) {
      registry->entries[i].active = false;
      found = true;
      break;
    }
  }

  pthread_mutex_unlock(&registry->mutex);
  return found;
}

bool is_subscription_active(subscription_registry_t* registry, uint32_t xapp_id, uint16_t ran_func_id)
{
  pthread_mutex_lock(&registry->mutex);
  bool active = false;

  for (size_t i = 0; i < registry->count; i++) {
    if (registry->entries[i].xapp_id == xapp_id && registry->entries[i].ran_func_id == ran_func_id) {
      active = registry->entries[i].active;
      break;
    }
  }

  pthread_mutex_unlock(&registry->mutex);
  return active;
}
