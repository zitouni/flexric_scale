/*
 * File: hiper_ran_tcp_client.h
 * Author: Rafik ZITOUNI
 * Date: 2025-02-03
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
#ifndef HIPER_RAN_TCP_CLIENT_H
#define HIPER_RAN_TCP_CLIENT_H

#include <pthread.h>

// Function declarations
void cleanup_tcp_client(void);
void force_close_socket(void);
pthread_t start_tcp_client(void);
void stop_tcp_client(void);
void send_ho_ok(void);

int send_message_to_server(const char* message);

#endif // HIPER_RAN_TCP_CLIENT_H