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

// this implements walker's algorithm for generating numbers from a
// discrete PMF, based on the following paper:
//
// Alastair J Walker, An efficient method for generating discrete
// random variables with general distributions, ACM Trans Math Soft
// 3, 253-256 (1977).

#include <assert.h>
#include <time.h>
#include <stack>
#include <krb/rng_discrete.hpp>
#include <krb/mt_rand.hpp>

// preprocess the PMF to compute F and A, which we'll use in sampling
rng_discrete_t::rng_discrete_t(const std::vector<double> &pmf)
  : K(pmf.size()), F(K), A(K)
{
  if(K == 1)
    return;

  mt_srand(time(0));

  // ensure PMF values are positive, and normalize
  uint32_t k;
  double sum = 0;
  for(k = 0; k < K; ++k) {
    assert(pmf[k] >= 0);
    sum += pmf[k];
  }

  // compute normalized PMF
  std::vector<double> E(K);
  for(k = 0; k < K; ++k)
    E[k] = pmf[k] / sum;

  // now divide into bigs and smalls
  double mean = 1.0 / K;
  std::stack<uint32_t> big, small;
  for(k = 0; k < K; ++k) {
    if(E[k] < mean)
      small.push(k);
    else
      big.push(k);
  }

  // preprocess
  uint32_t s, b;
  double d;
  while(!small.empty()) {

    s = small.top();
    small.pop();

    if(big.empty()) {
      A[s] = s;
      F[s] = 1.0;
      continue;
    }

    b = big.top();
    big.pop();
    A[s] = b;
    F[s] = K * E[s];

    d = mean - E[s];
    E[s] += d; // now E[s] == mean
    E[b] -= d;

    if(E[b] < mean)
      small.push(b);  // no longer big, add to small stack
    else if(E[b] > mean)
      big.push(b);    // still big, add it back to the stack
    else {            // E[b] == mean means we're done with this big
      A[b] = b;
      F[b] = 1.0;
    }

  }

  while(!big.empty()) {
    b = big.top();
    big.pop();
    A[b] = b;
    F[b] = 1.0;
  }

  // stacks emptied, A and F are filled

  // per Knuth, set F'[k] = (k + F[k]) / K to save a little math in
  // the sampling
  for(k = 0; k < K; ++k) {
    F[k] += k;
    F[k] /= K;
  }
}

uint32_t rng_discrete_t::sample() const
{
  if(K == 1)
    return 0;

  double u = mt_rand_0_1();
  uint32_t c = (uint32_t)(u * K);
  double f = F[c];

  if(f == 1.0)
    return c;

  if(u < f)
    return c;
  else
    return A[c];
}

double rng_discrete_t::probability_of(uint32_t k) const
{
  if(k > K)
    return 0;

  double f, p = 0;
  for(uint32_t i = 0; i < K; ++i) {
    f = K * F[i] - i;
    if(i == k)
      p += f;
    else if(k == A[i])
      p += 1.0 - f;
  }

  return p / K;
}
