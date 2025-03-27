/*
 * File: hiper_ran_tcp_client.c
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

/*
 * File: hiper_ran_tcp_client.c
 * TCP client implementation for hiper_ran_xapp
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <errno.h>

#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <fcntl.h>

#define NBR_DATA_COLLECTED 20

int count_collect_data = 0;

// Structure for the message thread
typedef struct {
  char* message;
  size_t length;
} message_thread_data_t;

// Modify your existing tcp_client_t structure
typedef struct {
  int sock_fd;
  volatile int is_running;
  pthread_mutex_t send_mutex; // Add mutex for thread-safe sending
} tcp_client_t;

// Modify the global client initialization
static tcp_client_t g_client = {.sock_fd = -1, .is_running = 0, .send_mutex = PTHREAD_MUTEX_INITIALIZER};

#define SERVER_IP "10.5.25.63" // Change this to your server IP
// #define SERVER_IP "127.0.0.1" // Change this to your server IP
#define SERVER_PORT 12346
// #define SERVER_PORT 12345
#define BUFFER_SIZE 4096

// Function to print received messages
void print_received_message(const char* message, size_t length)
{
  printf("\n=====> RU Controler Message with size: %zu is : %s\n", length, message);
  fflush(stdout);
}

// Function to initialize TCP client connection
void force_close_socket(void)
{
  pthread_mutex_lock(&g_client.send_mutex);
  if (g_client.sock_fd >= 0) {
    shutdown(g_client.sock_fd, SHUT_RDWR);
    close(g_client.sock_fd);
    g_client.sock_fd = -1;
  }
  g_client.is_running = 0;
  pthread_mutex_unlock(&g_client.send_mutex);
}

int init_tcp_client(void)
{
  struct sockaddr_in server_addr = {.sin_family = AF_INET, .sin_port = htons(SERVER_PORT), .sin_addr.s_addr = 0};

  // Create socket
  g_client.sock_fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
  if (g_client.sock_fd < 0) {
    perror("Socket creation failed");
    return -1;
  }

  // Set socket options
  int opt = 1;
  if (setsockopt(g_client.sock_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
    perror("setsockopt failed");
    close(g_client.sock_fd);
    g_client.sock_fd = -1;
    return -1;
  }

  // Convert IP address
  if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
    perror("Invalid address/Address not supported");
    close(g_client.sock_fd);
    g_client.sock_fd = -1;
    return -1;
  }

  // Connect to server
  if (connect(g_client.sock_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
    if (errno != EINPROGRESS) {
      perror("Connection failed");
      close(g_client.sock_fd);
      g_client.sock_fd = -1;
      return -1;
    }
  }

  g_client.is_running = 1;
  return 0;
}

int check_server_connection(int sock_fd)
{
  if (sock_fd < 0) {
    return 0;
  }

  struct tcp_info info;
  int len = sizeof(info);

  if (getsockopt(sock_fd, IPPROTO_TCP, TCP_INFO, &info, (socklen_t*)&len) == 0) {
    if (info.tcpi_state == TCP_ESTABLISHED) {
      return 1; // Connection is established
    }
  }

  return 0; // Connection is not established
}

// On The same port Number Receive/Transmit
int send_message_to_server(const char* message)
{
  if (!message || !g_client.is_running || g_client.sock_fd < 0) {
    return -1;
  }

  // Create a new thread to handle the sending
  // pthread_t thread_id;
  message_thread_data_t* thread_data = malloc(sizeof(message_thread_data_t));
  if (thread_data == NULL) {
    return -1;
  }

  // Copy the message
  thread_data->message = strdup(message);
  if (thread_data->message == NULL) {
    free(thread_data);
    return -1;
  }

  // Send the message in the current thread with mutex protection
  pthread_mutex_lock(&g_client.send_mutex);

  int result = 0;
  size_t message_len = strlen(message);
  size_t total_sent = 0;

  while (total_sent < message_len && g_client.is_running) {
    ssize_t sent = send(g_client.sock_fd, message + total_sent, message_len - total_sent, MSG_NOSIGNAL);

    if (sent < 0) {
      if (errno == EINTR) {
        continue;
      }
      result = -1;
      break;
    }

    if (sent == 0) {
      result = -1;
      break;
    }

    total_sent += (size_t)sent;
  }

  pthread_mutex_unlock(&g_client.send_mutex);

  // Clean up
  free(thread_data->message);
  free(thread_data);

  return result;
}
// Send message to the RU controler that DU Handover OK
void send_ho_ok(void)
{
  char message[256];
  snprintf(message, sizeof(message), "Turn Off RU ID: %d", 0);
  sleep(1);
  if (send_message_to_server(message) < 0) {
    printf("Failed to send Message to the RU controler \n");
  }
}
// TCP client thread function
void* tcp_client_thread(void* arg __attribute__((unused)))
{
  char buffer[BUFFER_SIZE];
  ssize_t bytes_received;
  fd_set read_fds;
  struct timeval tv;
  const int reconnect_delay = 1; // 1 second delay between reconnection attempts

  while (1) { // Outer loop for reconnection
    // Initialize TCP client
    if (init_tcp_client() < 0) {
      printf("Failed to initialize TCP client\n");
      sleep(reconnect_delay);
      continue;
    }
    printf("Connected to RU Controler Server Successfully\n");
    // Set socket to non-blocking mode
    int flags = fcntl(g_client.sock_fd, F_GETFL, 0);
    if (flags < 0) {
      perror("fcntl F_GETFL failed");
      force_close_socket();
      sleep(reconnect_delay);
      continue;
    }
    if (fcntl(g_client.sock_fd, F_SETFL, flags | O_NONBLOCK) < 0) {
      perror("fcntl F_SETFL failed");
      force_close_socket();
      sleep(reconnect_delay);
      return NULL;
    }

    // Main receive loop
    while (g_client.is_running) {
      FD_ZERO(&read_fds);
      FD_SET(g_client.sock_fd, &read_fds);

      // Set timeout for select
      tv.tv_sec = 0;
      tv.tv_usec = 5000; // 5ms timeout - reduced for better responsiveness

      int ready = select(g_client.sock_fd + 1, &read_fds, NULL, NULL, &tv);

      if (ready < 0) {
        if (errno == EINTR) {
          continue;
        }
        perror("select failed");
        break;
      }

      if (ready > 0 && FD_ISSET(g_client.sock_fd, &read_fds)) {
        pthread_mutex_lock(&g_client.send_mutex);
        bytes_received = recv(g_client.sock_fd, buffer, BUFFER_SIZE - 1, MSG_DONTWAIT);
        pthread_mutex_unlock(&g_client.send_mutex);

        if (bytes_received > 0) {
          buffer[bytes_received] = '\0';
          print_received_message(buffer, bytes_received);

          count_collect_data++;

          printf("count_collect_data : %d\n", count_collect_data);

          if (count_collect_data == NBR_DATA_COLLECTED) {
            // if (rd->ind.gtp.msg.ho_info.ho_complete == 1) {
            //   Send message to the RU controler that DU Handover OK
            send_ho_ok();
            count_collect_data = 0;
            printf("Handover OK sent to switch off the RU controller/ Source DU: and Target DU: \n");
            // cond_send_ho_ok = true;
          }
        } else if (bytes_received == 0) {
          printf("Server closed connection\n");
          break;
        } else {
          if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
            perror("recv failed");
            break;
          }
        }
      }
      // Minimal sleep to prevent busy waiting
      usleep(500); // 0.5ms sleep - reduced for better responsiveness
    }
    force_close_socket();
    // If client is no longer running, exit the thread
    if (!g_client.is_running) {
      break;
    }
    sleep(reconnect_delay); // Wait before reconnecting
  }

  return NULL;
}

// Function to start TCP client thread
pthread_t start_tcp_client(void)
{
  pthread_t thread_id;
  int ret = pthread_create(&thread_id, NULL, tcp_client_thread, NULL);
  if (ret != 0) {
    perror("Failed to create TCP client thread");
    return 0;
  }
  return thread_id;
}

// Function to stop TCP client
void stop_tcp_client(void)
{
  g_client.is_running = 0;
  if (g_client.sock_fd >= 0) {
    shutdown(g_client.sock_fd, SHUT_RDWR);
    close(g_client.sock_fd);
    g_client.sock_fd = -1;
  }
  pthread_mutex_destroy(&g_client.send_mutex);
}

// TCP client cleanup
void cleanup_tcp_client(void)
{
  force_close_socket();
  pthread_mutex_destroy(&g_client.send_mutex);
}
