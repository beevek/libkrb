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
#include <list>
#include <krb/resource_pool.hpp>

struct resource_t
{
  static int N;
  int my_N;
  resource_t()
  {
    my_N = ++N;
    printf("initialized #%d\n", my_N);
  }
  resource_t(const resource_t &r)
  {
    my_N = ++N;
    printf("copied #%d, initialized #%d\n", r.my_N, my_N);
  }
  ~resource_t()
  {
    printf("destroyed #%d\n", my_N);
  }
};

int resource_t::N = 0;

int main()
{
  resource_pool<resource_t> pool(10, 100);
  std::list<resource_t *> R;

  printf("fetching 70 resources\n");
  for(int i = 0; i < 70; ++i)
    R.push_back(pool.fetch());

  printf("releasing all the resources\n");
  std::list<resource_t *>::iterator j = R.begin();
  for(; j != R.end(); ++j) {
    printf("releasing #%d\n", (*j)->my_N);
    pool.release(*j);
  }

  printf("done releasing, going away\n");

  return 0;
}
