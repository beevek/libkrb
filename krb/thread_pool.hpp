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

/*
  Thread pool for running arbitrary blocking jobs in an asynchronous
  system, based on libevent signalling and callback mechanisms.

  Worker threads are agnostic to the kinds of jobs they run --- you
  just pass in a job, and it gets run.  When the job completes, a
  callback you specify is called with the results (in the main thread
  of execution).

  You can specify a pool sizing policy (see resource_pool.hpp), along
  with low and high watermarks for the number of threads to run.

  Jobs are specified using an object derived from the thread_pool_job
  abstract class, which must specify a run() function (which does the
  actual blocking processing) and a callback() function (which is
  called when the job completes).  A job object can use data members
  to specify input data and store output data as appropriate.
*/

#ifndef _KRB_THREAD_POOL_HPP
#define _KRB_THREAD_POOL_HPP

#include <inttypes.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <event.h>
#include <vector>
#include <queue>
#include <krb/resource_pool.hpp>
#include <krb/locker.hpp>
#include <krb/exceptions.hpp>


// base job class: you must derive from this to specify jobs for the
// thread pool.  set priority however makes sense for your
// application.
struct thread_pool_job
{
  thread_pool_job() : priority(0) {}
  virtual ~thread_pool_job() {}
  virtual void run() = 0;
  virtual void callback() {}
  int priority;
};


// details of these below:
class thread_pool_worker;
template <class Policy> class thread_resource_pool;

// simple queue with a mutex we can lock
typedef std::pair<thread_pool_job *, thread_pool_worker *>
  thread_pool_cq_item;
struct thread_pool_completed_queue : public std::queue<thread_pool_cq_item>
{
  thread_pool_completed_queue() { pthread_mutex_init(&mutex, NULL); }
  ~thread_pool_completed_queue() { pthread_mutex_destroy(&mutex); }
  pthread_mutex_t mutex;
};


// the main thread pool class
template <class Policy = basic_pool_policy>
class thread_pool
{
public:

  thread_pool
    (uint32_t low_watermark, uint32_t high_watermark,
     struct event_base *ev_base);
  ~thread_pool();

  bool schedule(thread_pool_job *job);
  uint32_t pending() const { return todo.size(); }

protected:

  bool run_some_jobs();

  thread_resource_pool<Policy> *pool;
  int pipefd[2];
  struct event_base *evbase;
  struct event ev;

  struct thread_pool_priority_cmp
  {
    bool operator()(thread_pool_job *a, thread_pool_job *b) const
    {
      return a->priority < b->priority;
    }
  };

  typedef std::priority_queue
    <thread_pool_job *,
     std::vector<thread_pool_job *>,
     thread_pool_priority_cmp> priority_queue_type;

  priority_queue_type todo;
  thread_pool_completed_queue done;

  template <class P> friend void thread_pool_catch(int, short, void *);
};


//////////////////////////////////////////////////////////////////////
// implementation details
//////////////////////////////////////////////////////////////////////

// worker thread class
struct thread_pool_worker
{
  thread_pool_worker();
  thread_pool_worker(const thread_pool_worker &W);
  ~thread_pool_worker();

  bool run_job(thread_pool_job *job);
  void init();
  bool cancel();

  // set by the resource pool's recycler
  int fd;
  thread_pool_completed_queue *Q;

  // managed by this class
  thread_pool_job *current_job;
  pthread_t tid;
  pthread_mutex_t job_mutex;
  pthread_cond_t job_cond;
};


// a pool of worker threads that sets the workers' pipe fd
template <class Policy>
class thread_resource_pool :
  public resource_pool<thread_pool_worker, Policy>
{
protected:
  typedef resource_pool<thread_pool_worker, Policy> base;
  int fd;
  thread_pool_completed_queue &Q;

  virtual void recycle(thread_pool_worker *w)
  {
    w->fd = fd;
    w->Q = &Q;
  }

public:
  thread_resource_pool
    (uint32_t low_watermark, uint32_t high_watermark,
     int pipefd, thread_pool_completed_queue &queue)
      : base(low_watermark, high_watermark),
        fd(pipefd), Q(queue) {}
};


//////////////////////////////////////////////////////////////////////
// thread_pool implementation details

// callback called by libevent when a job completes; this calls
// job->callback().  defined below.
template <class Policy>
void thread_pool_catch(int fd, short event, void *arg);

template <class Policy>
thread_pool<Policy>::thread_pool
  (uint32_t low_watermark, uint32_t high_watermark,
   struct event_base *ev_base)
    : evbase(ev_base)
{
  // create the pipe for communication from our worker threads, and
  // make both ends nonblocking
  int rv;
  if((rv = pipe(pipefd)) < 0 ||
     (rv = fcntl(pipefd[0], F_GETFL)) < 0 ||
     (rv = fcntl(pipefd[0], F_SETFL, rv | O_NONBLOCK)) < 0 ||
     (rv = fcntl(pipefd[1], F_GETFL)) < 0 ||
     (rv = fcntl(pipefd[1], F_SETFL, rv | O_NONBLOCK)) < 0)
  {
    throw strerror_exception("Creating thread pool pipe", errno);
  }

  // create the pool of threads
  pool = new thread_resource_pool<Policy>
    (low_watermark, high_watermark, pipefd[1], done);

  // set up a libevent callback for completed jobs
  event_set(&ev, pipefd[0], EV_READ | EV_PERSIST,
            &thread_pool_catch<Policy>, this);

  if(ev_base)
    event_base_set(ev_base, &ev);

  if((rv = event_add(&ev, 0)) != 0)
    throw strerror_exception("Creating thread pool callback event", rv);
}

template <class Policy>
thread_pool<Policy>::~thread_pool()
{
  // first, clobber our worker threads; this should block until all
  // the jobs are finished
  if(pool)
    delete pool;

  // destroy our event
  event_del(&ev);

  // close our pipe
  close(pipefd[0]);
  close(pipefd[1]);
}

template <class Policy>
bool thread_pool<Policy>::schedule(thread_pool_job *job)
{
  todo.push(job);
  return run_some_jobs();
}

template <class Policy>
bool thread_pool<Policy>::run_some_jobs()
{
  while(!todo.empty()) {

    // get a worker thread if one is available
    thread_pool_worker *W = pool->fetch();

    // are we out of threads (hit the high watermark)?
    if(!W)
      break;

    // take the highest priority job off the queue
    thread_pool_job *J = todo.top();
    todo.pop();

    // tell this worker to run the job
    if(!W->run_job(J))
      return false;
  }

  return true;
}

// thread_pool event callback for completed jobs
template <class Policy>
void thread_pool_catch
  (int fd, short event, void *arg)
{
  thread_pool<Policy> *P = (thread_pool<Policy> *)arg;

  // read from pipe fd
  char buf[32];
  int n;
  do {
    n = read(P->pipefd[0], buf, sizeof(buf));
  } while(n > 0 || (n < 0 && errno == EINTR));

  if(n < 0 && errno != EAGAIN)
    throw strerror_exception("Failed reading from thread pool pipe", errno);
  else {
    // lock completed jobs queue
    locker L(P->done.mutex);

    // pop items from the queue, release the associated threads back
    // into the pool, and call callbacks
    while(!P->done.empty()) {
      P->pool->release(P->done.front().second);
      P->done.front().first->callback();
      P->done.pop();
    }
  }

  // some threads were freed up; try run some jobs in case we have any
  // waiting
  P->run_some_jobs();
}


//////////////////////////////////////////////////////////////////////
// thread_pool_worker implementation details

// the actual worker thread function
void * thread_pool_worker_thread(void *arg)
{
  thread_pool_worker *W = (thread_pool_worker *)arg;
  int old_cancel_state, n;

  // allow ourselves to be canceled
  pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &old_cancel_state);

  // first time through, we need to lock our condition variable's
  // mutex
  pthread_mutex_lock(&W->job_mutex);

  while(1) {

    // wait for a new job to arrive
    pthread_cond_wait(&W->job_cond, &W->job_mutex);
    pthread_mutex_unlock(&W->job_mutex);

    // should we quit?
    pthread_testcancel();

    if(W->current_job == NULL)
      continue;

    // run the job
    W->current_job->run();

    // lock for our condition variable (we'll do cond_wait on the next
    // loop iteration); the rest of the stuff at the end of this
    // iteration needs to be atomic, so we can ensure the controlling
    // thread doesn't try to signal our condition variable until we're
    // ready for it.
    pthread_mutex_lock(&W->job_mutex);

    // put the job on the completed jobs queue
    locker L(W->Q->mutex);
    W->Q->push(thread_pool_cq_item(W->current_job, W));
    W->current_job = NULL;

    // write to the thread pool's pipe to signal we've finished a job
    do {
      n = write(W->fd, "", 1);
    } while(n < 0 && errno == EINTR);

  }

  return NULL;
}

// thread_pool_worker (and its thread function) can access its members
// and the thread_pool_job's members without worrying about critical
// sections, because it can safely assume the resource pool takes care
// of ensuring the data is only available to one thread at a time.

thread_pool_worker::thread_pool_worker()
{
  init();
}

thread_pool_worker::thread_pool_worker(const thread_pool_worker &W)
{
  init(); // don't make a copy at all, just create a new thread
}

thread_pool_worker::~thread_pool_worker()
{
  cancel();
}

bool thread_pool_worker::run_job(thread_pool_job *job)
{
  if(current_job)
    return false;

  current_job = job;

  // signal our thread that it's got something to do
  if(pthread_mutex_lock(&job_mutex) != 0 ||
     pthread_cond_signal(&job_cond) != 0 ||
     pthread_mutex_unlock(&job_mutex) != 0)
  {
    return false;
  }

  return true;
}

void thread_pool_worker::init()
{
  fd = -1;
  Q = NULL;
  current_job = NULL;

  // set up the condition which will be signalled to tell our thread
  // there's a job waiting for it (or it's time for it to cancel)
  int rv;
  if((rv = pthread_mutex_init(&job_mutex, 0)) != 0 ||
     (rv = pthread_cond_init(&job_cond, 0)) != 0)
  {
    throw strerror_exception("Initializing thread pool job mutex/cond", rv);
  }

  // FIXME: should we allow setting of sigmask for the worker threads?
  // what about stacksize?  (maybe pass in an attr struct and a
  // sigmask?)

  // start the thread
  if((rv = pthread_create(&tid, 0, thread_pool_worker_thread, this)) != 0)
    throw strerror_exception("Creating thread pool worker", rv);
}

bool thread_pool_worker::cancel()
{
  void *rv;
  return (pthread_cancel(tid) == 0 &&
          pthread_join(tid, &rv) == 0);
}


#endif // _KRB_THREAD_POOL_HPP
