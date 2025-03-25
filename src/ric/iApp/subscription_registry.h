/*
 * File: subscription_registry.h
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
#ifndef SUBSCRIPTION_REGISTRY_H
#define SUBSCRIPTION_REGISTRY_H

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>

typedef struct {
  uint32_t xapp_id;
  uint16_t ran_func_id;
  uint16_t ric_req_id;
  bool active;
} subscription_entry_t;

typedef struct {
  subscription_entry_t* entries;
  size_t count;
  size_t capacity;
  pthread_mutex_t mutex;
} subscription_registry_t;

void init_subscription_registry(subscription_registry_t* registry);
void free_subscription_registry(subscription_registry_t* registry);
bool register_subscription(subscription_registry_t* registry, uint32_t xapp_id, uint16_t ran_func_id, uint16_t ric_req_id);
bool deactivate_subscription(subscription_registry_t* registry, uint32_t xapp_id, uint16_t ran_func_id);
bool is_subscription_active(subscription_registry_t* registry, uint32_t xapp_id, uint16_t ran_func_id);

#endif // SUBSCRIPTION_REGISTRY_H
