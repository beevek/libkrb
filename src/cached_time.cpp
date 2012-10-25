/*
  Copyright 2008-2012 Kristopher R Beevers and Internap Network
  Services Corporation.

  Permission is hereby granted, free of charge, to any person
  obtaining a copy of this software and associated documentation files
  (the "Software"), to deal in the Software without restriction,
  including without limitation the rights to use, copy, modify, merge,
  publish, distribute, sublicense, and/or sell copies of the Software,
  and to permit persons to whom the Software is furnished to do so,
  subject to the following conditions:

  The above copyright notice and this permission notice shall be
  included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
  BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
  ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
  CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
*/

#include <krb/cached_time.hpp>
#include <unistd.h>

// thread entry point
void * cached_time_processor(void *arg)
{
  cached_time *C = (cached_time *)arg;

  // allow ourselves to be canceled
  int old_cancel_state;
  pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &old_cancel_state);

  while(1) {
    pthread_testcancel();

    // wait for the timeout
    usleep(C->interval_usec);

    // mark cached data stale; now the next time either time() or
    // gettimeofday() is called, it'll refresh the data then cause us
    // to loop again.
    C->stale_time = true;
    C->stale_gettimeofday = true;

    // wait for one of time() or gettimeofday() to be called before
    // looping again.
    pthread_mutex_lock(&C->renew_mutex);
    pthread_cond_wait(&C->renew_cond, &C->renew_mutex);
    pthread_mutex_unlock(&C->renew_mutex);
  }
}

cached_time::cached_time(uint32_t interval_ms)
  : interval_usec(interval_ms * 1000),
    stale_time(true), stale_gettimeofday(true)
{
  // set up the condition which will be signaled to tell our thread it
  // needs to stale-ify our cached values after the timeout
  pthread_mutex_init(&renew_mutex, 0);
  pthread_cond_init(&renew_cond, 0);

  // set up the time worker thread
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

  // allocate a minimal stack as we hardly use the stack in this
  // thread; this is PTHREAD_STACK_MIN (16384 on most systems) + 512
  pthread_attr_setstacksize(&attr, 16896);

  // start the thread
  pthread_create(&tid, &attr, cached_time_processor, this);

  pthread_attr_destroy(&attr);
}

cached_time::~cached_time()
{
  if(tid) {
    pthread_cancel(tid);
    pthread_cond_broadcast(&renew_cond);
  }
}

time_t cached_time::time()
{
  if(stale_time) {
    c_time = ::time(NULL);
    stale_time = false;
    pthread_cond_broadcast(&renew_cond);
  }

  return c_time;
}

int cached_time::gettimeofday(struct timeval *tv)
{
  if(stale_gettimeofday) {
    c_gettimeofday_rv = ::gettimeofday(&c_gettimeofday, NULL);
    stale_gettimeofday = false;
    pthread_cond_broadcast(&renew_cond);
  }

  tv = &c_gettimeofday;
  return c_gettimeofday_rv;
}
