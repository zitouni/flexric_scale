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



#include "asio_xapp.h"
#include <assert.h>                        // for assert
#include <bits/types/struct_itimerspec.h>  // for itimerspec
#include <errno.h>                         // for errno
#include <fcntl.h>                         // for fcntl, F_GETFL, F_SETFL
#include <stdio.h>                         // for NULL, printf, fflush, stdout
#include <string.h>                        // for strerror
#include <sys/epoll.h>                     // for epoll_event, epoll_ctl
#include <sys/time.h>                      // for CLOCK_MONOTONIC
#include <sys/timerfd.h>                   // for timerfd_create, timerfd_se...
#include <time.h>                          // for time_t, timespec
#include <unistd.h>                        // for close

static
void set_fd_non_blocking(int sfd)
{
  int flags = fcntl (sfd, F_GETFL, 0);  
  flags |= O_NONBLOCK;
  fcntl (sfd, F_SETFL, flags);
}

void init_asio_xapp(asio_xapp_t* io)
{
  assert(io != NULL);
  const int flags = EPOLL_CLOEXEC; 
  const int efd = epoll_create1(flags);  
  assert(efd != -1);
  io->efd = efd;
}

void add_fd_asio_xapp(asio_xapp_t* io, int fd)
{
  assert(io != NULL);
  assert(fd > 0 && "fd cannot be negative, data corrupted");

  set_fd_non_blocking(io->efd);
  const int op = EPOLL_CTL_ADD;
  const epoll_data_t e_data = {.fd = fd};
  const int e_events = EPOLLIN; // open for reading
  struct epoll_event event = {.events = e_events, .data = e_data};
  int rc = epoll_ctl(io->efd, op, fd, &event);
  assert(rc != -1);
}

int create_timer_ms_asio_xapp(asio_xapp_t* io, long initial_ms, long interval_ms)
{
  assert(io != NULL);
  assert(initial_ms > 0);
  assert(interval_ms > 0);

  // Create the timer
  const int clockid = CLOCK_MONOTONIC;
  const int flags = TFD_NONBLOCK | TFD_CLOEXEC;
  const int tfd = timerfd_create(clockid, flags);
  assert(tfd != -1);

  const time_t initial_sec = initial_ms / 1000;
  const long initial_nsec = (initial_ms * 1000000) % 1000000000;
  /* Initial expiration */
  const struct timespec it_value = {.tv_sec = initial_sec, .tv_nsec = initial_nsec};

  const time_t interval_sec = interval_ms / 1000;
  const long interval_nsec = (interval_ms * 1000000) % 1000000000;
  /* Interval for periodic timer */
  const struct timespec it_interval = {.tv_sec = interval_sec, .tv_nsec = interval_nsec};

  const int flags_2 = 0;
  struct itimerspec *old_value = NULL; // not interested in how the timer was previously configured
  const struct itimerspec new_value = {.it_interval = it_interval, .it_value = it_value};
  int rc = timerfd_settime(tfd, flags_2, &new_value, old_value);
  assert(rc != -1);

//  printf("Adding fd = %d into the RIC\n", tfd);
  add_fd_asio_xapp(io, tfd);
  return tfd;
}

void rm_fd_asio_xapp(asio_xapp_t* io, int fd)
{
  assert(io != NULL);
  const int op = EPOLL_CTL_DEL;
  const epoll_data_t e_data = {.fd = fd};
  const int e_events = EPOLLIN; // open for reading
  struct epoll_event event = {.events = e_events, .data = e_data};
  int rc = epoll_ctl(io->efd, op, fd, &event);
  assert(rc != -1);
  rc = close(fd);
  assert(rc == 0);

}

// int event_asio_xapp(asio_xapp_t const* io)
// {
//   assert(io != NULL);

//   const int maxevents = 1;
//   struct epoll_event events[maxevents];
//   const int timeout_ms = 1000;

//   const int events_ready = epoll_wait(io->efd, events, maxevents, timeout_ms); 
//   if(events_ready == -1){
//     printf("Error detected = %s \n", strerror(errno));
//     fflush(stdout);
//   }
//   assert(events_ready == 0 || events_ready == 1);

//   if(events_ready == 0) return -1;

//   // Max. one event ready
//   assert((events[0].events & EPOLLERR) == 0);
//   return events[0].data.fd; 
// }

int event_asio_xapp(asio_xapp_t const* io)
{
    assert(io != NULL);

    const int maxevents = 1;
    struct epoll_event events[maxevents];
    const int timeout_ms = 1000;

    int events_ready;
    do {
        events_ready = epoll_wait(io->efd, events, maxevents, timeout_ms);
    } while (events_ready == -1 && errno == EINTR);

    if (events_ready == -1) {
        printf("Error detected = %s \n", strerror(errno));
        fflush(stdout);
    }
    assert(events_ready == 0 || events_ready == 1);

    if (events_ready == 0) return -1;

    // Max. one event ready
    assert((events[0].events & EPOLLERR) == 0);
    return events[0].data.fd;
}
