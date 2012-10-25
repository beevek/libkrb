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
#include <krb/bloom_filter.hpp>
#include <krb/counting_bloom_filter.hpp>

#define URL_MAX 2048
#define URL_MAX_STR "2048"

int main(int argc, char **argv)
{
  uint32_t device_id;
  char url[URL_MAX];
  uint32_t num_inserts = 0;

  if(argc < 3) {
    printf("Usage: bloom <num_elements> <false_pos_rate> [num_inserts]\n");
    return 1;
  }

  if(argc == 4)
    num_inserts = atoi(argv[3]);

  uint32_t N = atoi(argv[1]);
  double fpr = atof(argv[2]);

  bloom_filter F(N, fpr);
  counting_bloom_filter<> C(N, fpr);

  printf("buckets: %u\nhashes: %u\n", F.buckets(), F.hashes());

  uint32_t total = 0;
  while(fscanf(stdin, "%u %" URL_MAX_STR "s\n", &device_id, url) == 2) {
    F.add(url, strlen(url));
    assert(F.query(url, strlen(url)));

    C.add(url, strlen(url));
    assert(C.query(url, strlen(url)));

    ++total;
    if(num_inserts && total >= num_inserts)
      break;
  }
  printf("%u inserts\n", total);


  bloom_filter F2(N, fpr);

  uint32_t fp = 0, fpc = 0, del = 0;
  total = 0;
  if(num_inserts) {
    // test for false positives assuming all remaining urls are unique
    while(fscanf(stdin, "%u %" URL_MAX_STR "s\n", &device_id, url) == 2) {

      if(F.query(url, strlen(url)))
        ++fp;

      if(C.query(url, strlen(url)))
        ++fpc;

      ++total;

      // also add into a second filter to later test merge
      F2.add(url, strlen(url));

      // or in the case of counting filter, try deleting
      if(C.remove(url, strlen(url)))
        ++del;
    }
    printf("normal: %u false positives\n%f FP rate\n", fp, double(fp)/double(total));
    printf("counting: %u false positives\n%f FP rate\n%u deletes\n",
           fpc, double(fpc)/double(total), del);
  }

  // try this
  F.merge(F2);

  return 0;
}
