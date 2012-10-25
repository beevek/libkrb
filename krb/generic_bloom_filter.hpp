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
  Generic templated Bloom filter type.  Template parameters are
  BackingStore --- the actual data store for the filter, which enables
  us to implement different kinds of filters; and Hasher --- a hash
  table function object, which defaults to murmur_hash.

  BackingStore must implement the following methods:

    resize(n): resize to accommodate n buckets
    size_t size() const: number of buckets available
    reset(): reset the set to empty
    set(n): mark bucket n occupied according to the backing store's
      mechanism
    bool test(n): return true if bucket n is occupied
    operator|=(F): merge the filter's values with that of F

  Generally you won't want to use this class, you'll just want one of
  the provided Bloom filters with already-implemented backing stores:

    bloom_filter.hpp
    counting_bloom_filter.hpp
    time_decay_bloom_filter.hpp
*/

#ifndef _KRB_GENERIC_BLOOM_FILTER_HPP
#define _KRB_GENERIC_BLOOM_FILTER_HPP

#include <inttypes.h>
#include <krb/murmur_hash.hpp>

template <class BackingStore, class Hasher = murmur_hash>
class generic_bloom_filter
{

public:

  generic_bloom_filter(uint32_t num_elements, double false_positive_rate);

  uint32_t buckets() const;
  uint32_t hashes() const;

  void reset();

  void add(const void *key, uint32_t sz);
  bool query(const void *key, uint32_t sz) const;

  bool merge(const generic_bloom_filter &F);

protected:

  uint32_t B; // buckets per element
  uint32_t K; // number of hash functions

  BackingStore store;
  Hasher hash_func;

  uint32_t get_next_bucket
    (const void *key, uint32_t sz, uint32_t &seed) const;

  static const uint32_t max_B = 33;
  static const uint32_t max_K = 8;
  static const uint32_t optimal_k_per_bucket[max_B];
  static const double false_positive_rates[max_B][max_K+1];
  void compute_k_and_b(double max_fp_rate);

};


//////////////////////////////////////////////////////////////////////
// implementation details
//////////////////////////////////////////////////////////////////////

template <class BackingStore, class Hasher>
generic_bloom_filter<BackingStore, Hasher>::generic_bloom_filter
  (uint32_t num_elements, double false_positive_rate)
{
  compute_k_and_b(false_positive_rate);
  store.resize(num_elements * B);
}

template <class BackingStore, class Hasher>
uint32_t generic_bloom_filter<BackingStore, Hasher>::buckets() const
{
  return store.size();
}

template <class BackingStore, class Hasher>
uint32_t generic_bloom_filter<BackingStore, Hasher>::hashes() const
{
  return K;
}

template <class BackingStore, class Hasher>
void generic_bloom_filter<BackingStore, Hasher>::reset()
{
  store.reset();
}

template <class BackingStore, class Hasher>
void generic_bloom_filter<BackingStore, Hasher>::add
  (const void *key, uint32_t sz)
{
  uint32_t H = 0;
  for(uint32_t i = 0; i < K; ++i)
    store.set(get_next_bucket(key, sz, H));
}

template <class BackingStore, class Hasher>
bool generic_bloom_filter<BackingStore, Hasher>::query
  (const void *key, uint32_t sz) const
{
  uint32_t H = 0;
  for(uint32_t i = 0; i < K; ++i)
    if(!store.test(get_next_bucket(key, sz, H)))
      return false;
  return true;
}

template <class BackingStore, class Hasher>
bool generic_bloom_filter<BackingStore, Hasher>::merge
  (const generic_bloom_filter<BackingStore, Hasher> &F)
{
  if(store.size() != F.store.size() || F.B != B || F.K != K)
    return false;

  store |= F.store;
  return true;
}

template <class BackingStore, class Hasher>
uint32_t generic_bloom_filter<BackingStore, Hasher>::get_next_bucket
  (const void *key, uint32_t sz, uint32_t &seed) const
{
  seed = hash_func(key, sz, seed);
  return (seed % store.size());
}

// tables for determining optimal values of K and B.  calculations are
// from http://www.cs.wisc.edu/~cao/papers/summary-cache/node8.html
// ("Bloom Filters - the math") via cassandra's bloom filter
// implementation.
template <class BackingStore, class Hasher>
const uint32_t
generic_bloom_filter<BackingStore, Hasher>::optimal_k_per_bucket
  [generic_bloom_filter<BackingStore, Hasher>::max_B] = 
  { 1, 1, 1, 2, 3, 3, 4, 5, 5, 6, 7, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8 };

template <class BackingStore, class Hasher>
const double
generic_bloom_filter<BackingStore, Hasher>::
  false_positive_rates
    [generic_bloom_filter<BackingStore, Hasher>::max_B]
    [generic_bloom_filter<BackingStore, Hasher>::max_K+1] = 
{
  {1.0},
  {1.0, 1.0},
  {1.0, 0.393, 0.400},
  {1.0, 0.283, 0.237, 0.253},
  {1.0, 0.221, 0.155, 0.147, 0.160},
  {1.0, 0.181, 0.109, 0.092, 0.092, 0.101},
  {1.0, 0.154, 0.0804, 0.0609, 0.0561, 0.0578, 0.0638},
  {1.0, 0.133, 0.0618, 0.0423, 0.0359, 0.0347, 0.0364},
  {1.0, 0.118, 0.0489, 0.0306, 0.024, 0.0217, 0.0216, 0.0229},
  {1.0, 0.105, 0.0397, 0.0228, 0.0166, 0.0141, 0.0133, 0.0135, 0.0145},
  {1.0, 0.0952, 0.0329, 0.0174, 0.0118, 0.00943, 0.00844, 0.00819, 0.00846},
  {1.0, 0.0869, 0.0276, 0.0136, 0.00864, 0.0065, 0.00552, 0.00513, 0.00509},
  {1.0, 0.08, 0.0236, 0.0108, 0.00646, 0.00459, 0.00371, 0.00329, 0.00314},
  {1.0, 0.074, 0.0203, 0.00875, 0.00492, 0.00332, 0.00255, 0.00217, 0.00199},
  {1.0, 0.0689, 0.0177, 0.00718, 0.00381, 0.00244, 0.00179, 0.00146, 0.00129},
  {1.0, 0.0645, 0.0156, 0.00596, 0.003, 0.00183, 0.00128, 0.001, 0.000852},
  {1.0, 0.0606, 0.0138, 0.005, 0.00239, 0.00139, 0.000935, 0.000702, 0.000574},
  {1.0, 0.0571, 0.0123, 0.00423, 0.00193, 0.00107, 0.000692, 0.000499, 0.000394},
  {1.0, 0.054, 0.0111, 0.00362, 0.00158, 0.000839, 0.000519, 0.00036, 0.000275},
  {1.0, 0.0513, 0.00998, 0.00312, 0.0013, 0.000663, 0.000394, 0.000264, 0.000194},
  {1.0, 0.0488, 0.00906, 0.0027, 0.00108, 0.00053, 0.000303, 0.000196, 0.00014},
  {1.0, 0.0465, 0.00825, 0.00236, 0.000905, 0.000427, 0.000236, 0.000147, 0.000101},
  {1.0, 0.0444, 0.00755, 0.00207, 0.000764, 0.000347, 0.000185, 0.000112, 7.46e-05},
  {1.0, 0.0425, 0.00694, 0.00183, 0.000649, 0.000285, 0.000147, 8.56e-05, 5.55e-05},
  {1.0, 0.0408, 0.00639, 0.00162, 0.000555, 0.000235, 0.000117, 6.63e-05, 4.17e-05},
  {1.0, 0.0392, 0.00591, 0.00145, 0.000478, 0.000196, 9.44e-05, 5.18e-05, 3.16e-05},
  {1.0, 0.0377, 0.00548, 0.00129, 0.000413, 0.000164, 7.66e-05, 4.08e-05, 2.42e-05},
  {1.0, 0.0364, 0.0051, 0.00116, 0.000359, 0.000138, 6.26e-05, 3.24e-05, 1.87e-05},
  {1.0, 0.0351, 0.00475, 0.00105, 0.000314, 0.000117, 5.15e-05, 2.59e-05, 1.46e-05},
  {1.0, 0.0339, 0.00444, 0.000949, 0.000276, 9.96e-05, 4.26e-05, 2.09e-05, 1.14e-05},
  {1.0, 0.0328, 0.00416, 0.000862, 0.000243, 8.53e-05, 3.55e-05, 1.69e-05, 9.01e-06},
  {1.0, 0.0317, 0.0039, 0.000785, 0.000215, 7.33e-05, 2.97e-05, 1.38e-05, 7.16e-06},
  {1.0, 0.0308, 0.00367, 0.000717, 0.000191, 6.33e-05, 2.5e-05, 1.13e-05, 5.73e-06}
};

// given a desired maximum false positive rate, pick K and B to
// achieve that rate, from our table above.  we want to minimize both
// K and B.  we give preference to minimizing storage over minimizing
// computation.
template <class BackingStore, class Hasher>
void generic_bloom_filter<BackingStore, Hasher>::compute_k_and_b
  (double max_fp_rate)
{
  // initial values: minimum K and B
  B = 2;
  K = optimal_k_per_bucket[B];

  // edge case: large allowable error rate
  if(max_fp_rate >= false_positive_rates[B][K])
    return;

  // edge case: tiny allowable error rate
  if(max_fp_rate < false_positive_rates[max_B-1][max_K]) {
    B = max_B;
    K = max_K;
    return;
  }

  // normal case: first find minimal number of buckets
  while(false_positive_rates[B][K] > max_fp_rate) {
    ++B;
    K = optimal_k_per_bucket[B];
  }

  // now try and reduce K a little without increasing false positives
  // too much
  while(false_positive_rates[B][K-1] <= max_fp_rate)
    --K;
}

#endif // _KRB_GENERIC_BLOOM_FILTER_HPP
