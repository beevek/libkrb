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
  Caching time_t/timeval thread.

  The time cacher starts a separate thread which sleeps for a
  sub-second interval between calls to time() and/or gettimeofday().
  the accuracy of time() is essentially unaffected as long as the
  sleep interval is small enough, but the resolution of gettimeofday()
  is limited to the refresh interval.

  This class can only be instantiated once.  To do so, call
  cached_time::init(), which returns a reference to the instantiation.
*/

#ifndef _CACHED_TIME_HPP
#define _CACHED_TIME_HPP

#include <inttypes.h>
#include <sys/time.h>
#include <time.h>
#include <pthread.h>

class cached_time
{
public:

  // interval_ms should be less than 1000; gettimeofday()'s accuracy
  // is limited by interval_ms --- you need to choose a tradeoff
  // between the frequency at which you need the time and the accuracy
  // you want.
  static cached_time & init(uint32_t interval_ms = 500)
  {
    static cached_time singleton(interval_ms);
    return singleton;
  }

  ~cached_time();

  // analogous to calling libc time(NULL)
  time_t time();

  // analogous to calling libc gettimeofday(tv, NULL)
  int gettimeofday(struct timeval *tv);

protected:

  cached_time(uint32_t interval_ms); // private constructor
  cached_time(const cached_time &);  // no copy construction
  cached_time & operator=(const cached_time &); // no assignment

  uint32_t interval_usec;
  pthread_mutex_t renew_mutex;
  pthread_cond_t renew_cond;
  pthread_t tid;

  time_t c_time;
  struct timeval c_gettimeofday;
  int c_gettimeofday_rv;

  bool stale_time, stale_gettimeofday;

  friend void * cached_time_processor(void *arg);

};

#endif // _CACHED_TIME_HPP
