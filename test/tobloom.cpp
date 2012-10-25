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

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <iostream>
#include <krb/timeout_bloom_filter.hpp>
#include <krb/apache_log_playback.hpp>


uint32_t hits = 0, misses = 0;
time_t now = 0;

struct to_bloom_adder : public apache_log_callback
{
  to_bloom_adder(timeout_bloom_filter &tobf, uint32_t timeout)
    : F(tobf), qmode(false), to(timeout) {}

  bool operator()
    (const apache_log_playback &p,
     const apache_log_entry &e)
  {
    if(qmode) {
      if(F.query(e.url(), strlen(e.url()), e.time(), to))
        ++hits;
      else
        ++misses;
    } else
      F.add(e.url(), strlen(e.url()), e.time());
    now = e.time();
    return true;
  }

  void query_mode(bool state) { qmode = state; }

protected:

  timeout_bloom_filter &F;
  bool qmode;
  uint32_t to;
};

int main(int argc, char **argv)
{
  uint32_t num_inserts = 0;

  if(argc < 4) {
    printf("Usage: tobloom <num_elements> <false_pos_rate> <timeout> [num_inserts]\n");
    return 1;
  }

  uint32_t E = atoi(argv[1]);
  uint32_t fpr = atof(argv[2]);
  uint32_t T = atoi(argv[3]);

  if(argc == 5)
    num_inserts = atoi(argv[4]);

  timeout_bloom_filter F(E, fpr);
  to_bloom_adder callback(F, T);

  apache_log_playback P(std::cin, callback, 20000);

  // if num_inserts is set, we'll insert a bunch of stuff into the
  // filter, then query the filter with subsequent request urls,
  // request times, and the given timeout, and count hits and misses.
  if(num_inserts > 0) {
    while(P.single_entry())
      if(P.line() >= num_inserts)
        break;
    callback.query_mode(true);
  }

  P.all_entries();

  printf("total requests: %u\n", P.line());
  if(num_inserts > 0)
    printf("after %u requests:\n  hits: %u\n  misses: %u\n",
           num_inserts, hits, misses);

  // also while we're at it, here's a very simple test:
  F.add("asdfasdf", 8, now);
  assert(!F.query("asdfasdf", 8, now+60, 59));
  assert(F.query("asdfasdf", 8, now+60, 60));

  return 0;
}
