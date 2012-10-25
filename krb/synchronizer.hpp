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

#ifndef _KRB_SYNCHRONIZER_HPP
#define _KRB_SYNCHRONIZER_HPP

#include <krb/exceptions.hpp>

class synchronizer_t
{
public:

  // thread_count should be the number of _worker_ threads (number of
  // readers).  we'll assume there's exactly one writer thread.
  synchronizer_t(int thread_count = 0)
    : num_threads(thread_count), do_wait(false), initialized(false)
  {}

  // to be called by a new worker thread on initialization if the
  // number of worker threads is _not_ passed to the constructor.
  // note that after the first acquire_sync() call is made, no further
  // threads may be added!  you need to make sure none of your threads
  // calls acquire_sync() immediately upon entrance; if so, you should
  // call add_thread() for each thread prior to actually starting the
  // thread.
  void add_thread()
  {
    if(initialized) {
      throw string_exception
        ("Attempt to add threads to an"
         " already-initialized synchronizer");
    }

#ifdef __GNUC__
    __sync_fetch_and_add(&num_threads, 1);
#else
    ++num_threads;
#endif
  }

  ~synchronizer_t()
  {
    if(initialized) {
      pthread_barrier_destroy(&start_barrier);
      pthread_barrier_destroy(&end_barrier);
    }
  }

  // signal workers to pause at the top of their processing loop and
  // wait for a signal before continuing, so that we can replace
  // shared data.  note that if the workers are in the midst of
  // processing or waiting for data or something like that, this could
  // take a long time.  this function should be called by the writing
  // thread when it's ready to do its writing.
  void acquire_sync()
  {
    if(!initialized) {
      // initialize once, under the assumption that add_thread will
      // not be called anymore once we start doing syncs
      pthread_barrier_init(&start_barrier, 0, num_threads + 1);
      pthread_barrier_init(&end_barrier, 0, num_threads + 1);
      initialized = true;
    }

#ifdef __GNUC__
    // memory barrier prevents compiler or hardware from reordering
    // the below assignment with respect to the above ones
    __sync_synchronize();
#endif

    do_wait = true; // signal to workers to synchronize

#ifdef __GNUC__
    // one more memory barrier
    __sync_synchronize();
#endif

    pthread_barrier_wait(&start_barrier);
  }

  // allow workers to continue on with their normal processing.  this
  // function should be called by the writing thread after it's
  // finished doing its writing.
  void release_sync()
  {
    do_wait = false;
#ifdef __GNUC_
    // another memory barrier as above
    __sync_synchronize();
#endif
    pthread_barrier_wait(&end_barrier);
  }

  // this function should be called by the readers at the top of their
  // loop.  it will cause the reader to block _only_ if there is new
  // config data waiting to be written.  otherwise it does nothing.
  void wait_for_updates()
  {
    if(do_wait) {
      // first wait for the start barrier, after which the writer will
      // know it can go ahead and write
      pthread_barrier_wait(&start_barrier);

      // then wait for the end barrier, after which the writer has
      // finished all its writing and we're good to proceed
      pthread_barrier_wait(&end_barrier);
    }
  }
  

protected:
  int num_threads;
  volatile bool do_wait, initialized;
  pthread_barrier_t start_barrier, end_barrier;

};

#endif // _KRB_SYNCHRONIZER_HPP
