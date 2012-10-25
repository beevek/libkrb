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

#include <iostream>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <krb/apache_log_entry.hpp>
#include <krb/apache_log_playback.hpp>

time_t last_time = 0;
uint32_t total_skew = 0;

struct print_entry : public apache_log_callback
{
  bool operator()
    (const apache_log_playback &p,
     const apache_log_entry &e)
  {
    if(e.time() < last_time)
      total_skew += last_time - e.time();

    // prints "INO" if the entry arrived "in order" wrt the previous
    // request; "OOO" if the entry arrived "out of order" wrt the
    // previous request
    printf("%u %s %s\n", (uint32_t)e.time(), e.time() < last_time ? "OOO" : "INO", e.url());
    last_time = e.time();
    return true;
  }
};

int main(int argc, char **argv)
{
  uint32_t buf_sz = 0;
  double speed;

  if(argc < 3) {
    printf("Usage: apachelog <buffered entries> <speed>\n");
    return 1;
  }

  buf_sz = atoi(argv[1]);
  speed = atof(argv[2]);

  print_entry callback;
  apache_log_playback P(std::cin, callback, buf_sz, speed);
  P.all_entries();

  fprintf(stderr, "TOTAL TIME SKEW: %u seconds\n", total_skew);

  return 0;
}
