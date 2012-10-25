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

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <inttypes.h>
#include <krb/resource_pool.hpp>
#include <krb/thread_pool.hpp>

struct my_job : public thread_pool_job
{
  static int N;
  static int done;
  int my_N;
  resource_pool<my_job> *pool;

  // copy constructor is the same as the default constructor
  my_job() { my_N = ++N; }
  my_job(const my_job &j) { my_N = ++N; }

  void run()
  {
    printf("running job #%d\n", my_N);
    sleep(1);
  }

  void callback()
  {
    printf("done with job #%d, finished %d jobs\n", my_N, ++done);
    if(pool)
      pool->release(this);
  }
};

int my_job::N = 0;
int my_job::done = 0;

// resource pool that sets my_job's pool member to point to the pool
// on initialization/recycle
struct job_resource_pool : public resource_pool<my_job>
{
  typedef resource_pool<my_job> base;
  job_resource_pool(uint32_t low_watermark, uint32_t high_watermark)
    : base(low_watermark, high_watermark) {}

protected:
  virtual void recycle(my_job *j)
  {
    j->pool = this;
  }
};

int main(int argc, char **argv)
{
  if(argc < 3) {
    printf("Usage: %s <# jobs> <# threads>\n", argv[0]);
    return 1;
  }

  uint32_t N_jobs = atoi(argv[1]);
  uint32_t N_threads = atoi(argv[2]);

  // initialize libevent, which drives thread_pool's callback
  // mechanism
  struct event_base *ev_base = (event_base *)event_init();

  // we need the job pool here to be as big as the total number of
  // jobs we want to run since we don't actually start running jobs
  // until all of them are scheduled.  in practice we just want our
  // job pool to have a high watermark equal to the absolute maximum
  // number of simultaneously pending jobs we expect to have.
  job_resource_pool job_pool(10, N_jobs);

  // create a thread pool using the event base we were given, with at
  // least one always-running thread and up to N_threads
  // simultaneously allocated threads
  thread_pool<> T(1, N_threads, ev_base);

  // schedule a bunch of jobs.  actually, the first N_threads jobs
  // will be run immediately here.  only after event_dispatch() is
  // called will we start calling callbacks on job completion, and
  // reusing worker threads.
  for(int i = 0; i < 1000; ++i) {
    my_job *j = job_pool.fetch();
    if(j)
      T.schedule(j);
  }

  // run the event loop
  event_dispatch();

  return 0;
}
