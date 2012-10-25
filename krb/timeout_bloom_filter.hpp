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
  Timeout Bloom filter, which tracks set membership with timestamps so
  that you can check to see if an object was in the set within some
  timeout period.

  The interfaces for adding and querying keys are slightly different
  than those for the other Bloom filters --- when adding a key you
  must provide a timestamp; when querying one, you need to provide the
  current timestamp and a desired timeout for the key.
*/

#ifndef _KRB_TIMEOUT_BLOOM_FILTER_HPP
#define _KRB_TIMEOUT_BLOOM_FILTER_HPP

#include <sys/types.h>
#include <vector>
#include <limits>
#include <algorithm>
#include <krb/generic_bloom_filter.hpp>
#include <krb/murmur_hash.hpp>

class timeout_bloom_store : public std::vector<time_t>
{
protected:
  typedef std::vector<time_t> base;
  typedef std::vector<time_t>::iterator iterator;

public:

  void reset()
  {
    for(iterator i = base::begin(); i != base::end(); ++i)
      *i = 0;
  }

  void set(uint32_t n) {} // never called
  void set(uint32_t n, time_t time)
  {
    // in case of overflow, leave the bucket set at the max value
    (*this)[n] = time;
  }

  bool test(uint32_t n) const { return false; } // never called
  bool test(uint32_t n, time_t time, uint32_t timeout) const
  {
    return ((*this)[n] >= (time_t)(time - timeout));
  }

  timeout_bloom_store & operator|=(const timeout_bloom_store &S)
  {
    // guaranteed that this and S have the same size
    for(uint32_t i = 0; i < base::size(); ++i)
      (*this)[i] = std::max((*this)[i], S[i]);
    return *this;
  }

};

// this makes the generic base class protected because we don't want
// to expose add/query interfaces from the base class.  unfortunately
// it means we need to pass through other calls explicitly.

class timeout_bloom_filter :
  protected generic_bloom_filter<timeout_bloom_store, murmur_hash>
{
protected:
  typedef generic_bloom_filter<timeout_bloom_store, murmur_hash> base;

public:

  timeout_bloom_filter
    (uint32_t num_elements, double false_positive_rate)
      : base(num_elements, false_positive_rate) {}

  uint32_t buckets() const { return base::buckets(); }
  uint32_t hashes() const { return base::hashes(); }

  void reset() { return base::reset(); }

  void add(const void *key, uint32_t sz, time_t time);
  bool query
    (const void *key, uint32_t sz,
     time_t time, uint32_t timeout_sec) const;

  bool merge(const timeout_bloom_filter &F) { return base::merge(F); }

};


//////////////////////////////////////////////////////////////////////
// implementation details
//////////////////////////////////////////////////////////////////////

void timeout_bloom_filter::add
  (const void *key, uint32_t sz, time_t time)
{
  uint32_t H = 0;
  for(uint32_t i = 0; i < base::K; ++i)
    base::store.set(base::get_next_bucket(key, sz, H), time);
}

bool timeout_bloom_filter::query
  (const void *key, uint32_t sz,
   time_t time, uint32_t timeout_sec) const
{
  uint32_t H = 0;
  for(uint32_t i = 0; i < base::K; ++i)
    if(!base::store.test(base::get_next_bucket(key, sz, H), time, timeout_sec))
      return false;
  return true;
}

#endif // _KRB_TIMEOUT_BLOOM_FILTER_HPP
