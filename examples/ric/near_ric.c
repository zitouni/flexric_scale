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

#include "../../src/ric/near_ric_api.h"

#include <arpa/inet.h>
#include <assert.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <poll.h>
#include <time.h>
#include <unistd.h>

const uint16_t MAC_ran_func_id = 142;
const uint16_t RLC_ran_func_id = 143;
const uint16_t PDCP_ran_func_id = 144;
const uint16_t SLICE_ran_func_id = 145; // Not implemented yet
const uint16_t KPM_ran_func_id = 147;
const char *cmd = "5_ms";

volatile sig_atomic_t keep_running = 1;

// static void stop_and_exit()
// {
//   // Stop the RIC
//   stop_near_ric_api();

//   exit(EXIT_SUCCESS);
// }

// static  pthread_once_t once = PTHREAD_ONCE_INIT;

// pthread_t cleanup_thread;
volatile sig_atomic_t stop_signal_received = false;

void sig_handler(int signum)
{
  if (signum == SIGINT) {
    printf("\nCtrl+C pressed. Preparing to exit...\n");
    keep_running = 0;
  }
}

void force_exit(int signum)
{
  printf("Force exit triggered. The program will now terminate.\n");
  exit(signum);
}

void cleanup()
{
  printf("Cleaning up resources...\n");
  stop_near_ric_api();
  force_exit(0);
  // Add any other cleanup code here
}

// void* cleanup_function(void*)
// {
//     printf("Cleanup thread started...\n");
//     stop_near_ric_api();
//     printf("near_ric_api stopped.\n");

//     // Add any other cleanup code here

//     printf("Cleanup complete.\n");
//     return NULL;
// }

// void start_cleanup()
// {
//     printf("Starting cleanup process...\n");
//     if (pthread_create(&cleanup_thread, NULL, cleanup_function, NULL) != 0) {
//         fprintf(stderr, "Failed to create cleanup thread\n");
//         exit(EXIT_FAILURE);
//     }
// }

// void wait_for_cleanup()
// {
//     printf("Waiting for cleanup to complete...\n");
//     pthread_join(cleanup_thread, NULL);
//     printf("Cleanup thread joined.\n");
// }

int main(int argc, char *argv[])
{
  struct sigaction sa;
  sa.sa_handler = sig_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;

  if (sigaction(SIGINT, &sa, NULL) == -1) {
    perror("Error setting up signal handler");
    return EXIT_FAILURE;
  }

  fr_args_t args = init_fr_args(argc, argv);

  // Init the RIC
  init_near_ric_api(&args);

  while (keep_running) {
    poll(NULL, 0, 1000);
  }

  // start_cleanup();
  // wait_for_cleanup();

  // printf("Program exited. Verifying all threads have stopped...\n");

  // // Print the current process's threads
  // char cmd[100];
  // snprintf(cmd, sizeof(cmd), "ps -T -p %d", getpid());
  // system(cmd);

  // printf("If you see only one thread above, all threads have been successfully cleaned up.\n");

  // Perform cleanup
  cleanup();

  // printf("Program exited gracefully.\n");
  return EXIT_SUCCESS;
}
