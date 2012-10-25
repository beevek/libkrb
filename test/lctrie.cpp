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
  Test program for the set-membership LC-trie class.

  This program takes 3 required arguments, and 2 optional arguments:

  * <-4|-6>: indicate that the prefix and address files are for ipv4
    or ipv6, respectively

  * prefix-list: filename of a file containing a list of CIDR format
    prefixes, one per line, to be compiled into an LC-trie.  there
    should be no whitespace, no comments, nothing other than the CIDR
    entries in the file.  you should only include the most general
    prefixes; no network should be a subnet of another network in the
    list.  if this filename ends in ".cpl", it is assumed to be a
    precompiled LC-trie and loaded directly without any compilation.

  * address-list: a list of fully qualified addresses (not in CIDR
    format), each of which will be searched against the LC-trie.

  * repeat: if set, an integer indicating how many times to repeat the
    searches for the addresses in address-list, for use in examining
    performance.  (default 1.)

  * output.cpl: if specified, an output filename into which the
    compiled LC-trie will be written.  the filename should end in
    ".cpl".  the format is gzipped binary.

  Test data:

  You may use the subnet and address lists in the data directory for
  testing the LC-trie.  For example:

  $ ./lctrie -4 data/subnets4.kr data/addrs4.kr

  compiles an LC-trie containing all ipv4 subnets assigned to South
  Korea, and then tests a few addresses against it.  Good sources for
  country CIDR blocklists are
  http://www.countryipblocks.net/e_country_data/ and
  http://software77.net/geo-ip/

 */

#include <krb/lc_trie.hpp>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <vector>
#include <string>


static clock_t startclock, stopclock;
void clockon() { startclock = clock(); }
void clockoff() { stopclock = clock(); }
double gettime(void)
{
   return (stopclock-startclock) / (double) CLOCKS_PER_SEC;
}

template <class IPType>
void run(const char *pfile, const char *afile, int repeat = 1, const char *outfile = 0)
{
  std::string stats;
  int found = 0, notfound = 0;

  lc_trie<IPType> T;

  char *dot = (char *)strrchr(pfile, '.');
  if(dot != NULL && !strcasecmp(++dot, "cpl")) {
    clockon();
    if(!T.load(pfile)) {
      perror("failed loading precompiled trie");
      exit(1);
    }
    clockoff();
    printf("time to load precompiled trie: %f\n", gettime());
  } else {
    clockon();
    if(!compile_lc_trie<IPType>(pfile, T)) {
      fprintf(stderr, "failed compiling trie\n");
      exit(1);
    }
    clockoff();
    printf("compilation time: %f\n", gettime());
  }
  T.stats(stats);
  printf("trie stats: %s\n", stats.c_str());

  // save the trie if given an output filename
  if(outfile) {
    if(!T.save(outfile)) {
      perror("failed saving compiled trie");
      exit(1);
    }
  }

  // now load address file
  std::vector<IPType> addrs;
  char line[256];
  IPType ip;
  FILE *in = fopen(afile, "rb");
  if(!in) {
    perror("failed loading addresses");
    exit(1);
  }
  while(fscanf(in, "%256s", line) != EOF) {
    if(!strtoip<IPType>(line, &ip)) {
      fprintf(stderr, "can't convert '%s' to ip\n", line);
      exit(1);
    }
    addrs.push_back(ip);
  }

  // now for every address do a search on T and report stats
  clockon();
  for(int j = 0; j < repeat; ++j) {
    for(int i = 0; i < (int)addrs.size(); ++i) {
      if(T.search(addrs[i]))
        ++found;
      else
        ++notfound;
    }
  }
  clockoff();

  printf("searches: %d\nfound: %d\nnot found: %d\ntime: %f\n",
         repeat*addrs.size(), found, notfound, gettime());

}

int main(int argc, char **argv)
{
  if(argc < 4 || argv[1][0] == '\0') {
    // if the prefix-list filename ends in ".cpl" we'll assume it's a
    // precompiled list and load it directly
    fprintf(stderr, "Usage: %s <-4|-6> prefix-list[.cpl] address-list [repeat] [output.cpl]\n", argv[0]);
    return 1;
  }

  bool is_ipv4 = false;
  if(argv[1][1] == '4')
    is_ipv4 = true;
  else if(argv[1][1] != '6') {
    fprintf(stderr, "unknown address type '%c'\n", argv[1][1]);
    return 1;
  }

  int repeat = 1;
  if(argc >= 5)
    repeat = atoi(argv[4]);

  const char *outfile = 0;
  if(argc >= 6)
    outfile = argv[5];

  if(is_ipv4)
    run<ipv4>(argv[2], argv[3], repeat, outfile);
  else
    run<ipv6>(argv[2], argv[3], repeat, outfile);

  return 0;
}
