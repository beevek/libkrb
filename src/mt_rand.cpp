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

// quick little (borrowed) mersenne twister RNG implementation based
// on that of Makoto Matsumoto and Takuji Nishimura

#include <limits>
#include <krb/mt_rand.hpp>


/* Period parameters */  
#define N                       624
#define M                       397
#define MATRIX_A                0x9908b0df      /* constant vector a */
#define UPPER_MASK              0x80000000      /* most significant w-r bits */
#define LOWER_MASK              0x7fffffff      /* least significant r bits */

/* Tempering parameters */   
#define TEMPERING_MASK_B        0x9d2c5680
#define TEMPERING_MASK_C        0xefc60000
#define TEMPERING_SHIFT_U( y )  (y >> 11)
#define TEMPERING_SHIFT_S( y )  (y << 7)
#define TEMPERING_SHIFT_T( y )  (y << 15)
#define TEMPERING_SHIFT_L( y )  (y >> 18)

static uint32_t mt[N];                     /* the array for the state vector */
static int mti = N + 1;                         /* mti == N + 1 means mt[N] is not initialized */

/* Initializing the array with a seed */
void mt_srand(uint32_t seed)
{
  int i;

  for(i = 0; i < N; i++) {
    mt[i] = seed & 0xffff0000;
    seed = 69069 * seed + 1;
    mt[i] |= (seed & 0xffff0000) >> 16;
    seed = 69069 * seed + 1;
  }
  mti = N;
}

uint32_t mt_rand(void)
{
  uint32_t y;
  static uint32_t mag01[2] = { 0x0, MATRIX_A };
  /* mag01[x] = x * MATRIX_A  for x=0,1 */

  if(mti >= N) {        /* generate N words at one time */
    int kk;

    if(mti == N + 1)      /* if sgenrand() has not been called, */
      mt_srand(4357); /* a default initial seed is used */

    for(kk = 0; kk < N - M; kk++) {
      y = (mt[kk] & UPPER_MASK) | (mt[kk + 1] & LOWER_MASK);
      mt[kk] = mt[kk + M] ^ (y >> 1) ^ mag01[y & 0x1];
    }

    for(; kk < N - 1; kk++) {
      y = (mt[kk] & UPPER_MASK) | (mt[kk + 1] & LOWER_MASK);
      mt[kk] = mt[kk + (M - N)] ^ (y >> 1) ^ mag01[y & 0x1];
    }

    y = (mt[N - 1] & UPPER_MASK) | (mt[0] & LOWER_MASK);
    mt[N - 1] = mt[M - 1] ^ (y >> 1) ^ mag01[y & 0x1];

    mti = 0;
  }

  y = mt[mti++];
  y ^= TEMPERING_SHIFT_U( y );
  y ^= TEMPERING_SHIFT_S( y ) & TEMPERING_MASK_B;
  y ^= TEMPERING_SHIFT_T( y ) & TEMPERING_MASK_C;
  y ^= TEMPERING_SHIFT_L( y );

  return y; 
}

double mt_rand_0_1()
{
  return mt_rand() / double(std::numeric_limits<uint32_t>::max());
}
