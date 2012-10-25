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
#include <stdlib.h>
#include <iostream>
#include <krb/apache_log_playback.hpp>
#include <krb/lossy_hash_table.hpp>

typedef lossy_hash_table<const char *, uint32_t> lht_t;

struct url_counter : public apache_log_callback
{
  url_counter(lht_t &table) : T(table) {}

  bool operator()
    (const apache_log_playback &p,
     const apache_log_entry &e)
  {
    ++T[e.url()];
    return true;
  }

protected:

  lht_t &T;
};

int main(int argc, char **argv)
{
  if(argc < 2) {
    printf("Usage: lossyhash <size>\n");
    return 1;
  }

  uint32_t sz = atoi(argv[1]);
  lht_t T(sz);

  url_counter callback(T);
  apache_log_playback P(std::cin, callback, 20000);
  P.all_entries();

  lht_t::const_iterator i = T.begin();
  for(; i != T.end(); ++i)
    printf("%u ", *i);

  return 0;
}
