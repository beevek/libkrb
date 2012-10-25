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
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <krb/wss_estimator.hpp>
#include <krb/apache_log_playback.hpp>

struct wss_adder : public apache_log_callback
{
  wss_adder(wss_estimator<> &wsse, uint32_t interval_sec)
    : W(wsse), isec(interval_sec),
      intervals(0), last_interval(0) {}

  bool operator()
    (const apache_log_playback &p,
     const apache_log_entry &e)
  {
    if(last_interval == 0)
      last_interval = e.time();

    if(e.time() > last_interval && (uint32_t)(e.time() - last_interval) > isec) {
      // new interval
      W.end_interval();
      ++intervals;
      last_interval = e.time();
      printf("WSS after %u intervals: %llu (mem ~= %u)\n",
             intervals, W.size(), W.buckets()/8);
    }

    W.add(e.url(), strlen(e.url()), e.bytes());
    return true;
  }

protected:

  wss_estimator<> &W;
  uint32_t isec, intervals;
  time_t last_interval;
};


int main(int argc, char **argv)
{
  if(argc < 6) {
    printf("Usage: wss <num_intervals> <el_per_interval> <false_pos_rate> <adaptive_buf_perc>  <interval_sec>\n");
    return 1;
  }

  uint32_t N = atoi(argv[1]);
  uint32_t E = atoi(argv[2]);
  double fpr = atof(argv[3]);
  double afsb = atof(argv[4]);
  uint32_t isec = atoi(argv[5]);

  wss_estimator<> W(N, E, fpr, afsb);
  wss_adder callback(W, isec);

  apache_log_playback P(std::cin, callback, 20000);
  P.all_entries();

  return 0;
}
