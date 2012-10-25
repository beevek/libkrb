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
  Counting Bloom filter, which allows additions, removals, and
  queries.

  You can choose the counter size by supplying a Counter template
  argument, which should be a numeric type.

  FIXME: need to allow 3-4 bit counters, not just byte+ counters.
*/

#ifndef _KRB_COUNTING_BLOOM_FILTER_HPP
#define _KRB_COUNTING_BLOOM_FILTER_HPP

#include <vector>
#include <limits>
#include <krb/generic_bloom_filter.hpp>
#include <krb/murmur_hash.hpp>

template <class Counter>
class counting_bloom_store : public std::vector<Counter>
{
protected:
  typedef typename std::vector<Counter> base;
  typedef typename std::vector<Counter>::iterator iterator;

public:

  void reset()
  {
    for(iterator i = base::begin(); i != base::end(); ++i)
      *i = 0;
  }

  void set(uint32_t n)
  {
    // in case of overflow, leave the bucket set at the max value
    if((*this)[n] != std::numeric_limits<Counter>::max())
      ++(*this)[n];
  }

  bool test(uint32_t n) const
  {
    return ((*this)[n] > 0);
  }

  counting_bloom_store & operator|=(const counting_bloom_store &S)
  {
    // guaranteed that this and S have the same size
    for(uint32_t i = 0; i < base::size(); ++i) {
      if(std::numeric_limits<Counter>::max() - (*this)[i] < S[i])
        (*this)[i] = std::numeric_limits<Counter>::max(); // overflow
      else
        (*this)[i] += S[i];
    }
    return *this;
  }

};

template <class Counter = uint8_t>
class counting_bloom_filter :
  public generic_bloom_filter<counting_bloom_store<Counter>, murmur_hash>
{
protected:
  typedef generic_bloom_filter<counting_bloom_store<Counter>, murmur_hash> base;

public:

  counting_bloom_filter(uint32_t num_elements, double false_positive_rate)
    : base(num_elements, false_positive_rate) {}

  bool remove(const void *key, uint32_t sz)
  {
    if(!base::query(key, sz))
      return false; // can only delete keys that are in the set!

    uint32_t H = 0;
    for(uint32_t i = 0; i < base::K; ++i)
      --base::store[base::get_next_bucket(key, sz, H)];

    return true;
  }

};

#endif // _KRB_COUNTING_BLOOM_FILTER_HPP
