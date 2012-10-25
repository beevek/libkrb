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
  Simple templated lossy hash table class.  The table has a fixed
  number of hash buckets (set at construction time); multiple keys may
  hash to the same bucket, in which case the hash "loses" information.

  If your Keys are something other than the built-in types supported
  by murmur_hash_func, you'll have to define your own HashFunc.

  Technically I should probably make the base vector protected to
  avoid issues with any of the vector's member functions that may
  destroy the hash.  However, some of the vector's member functions
  can be useful (e.g., it's nice to be able to iterate over the values
  to compute statistics), so I'll leave it public and assume only
  smart people will use this class.
*/

#ifndef _KRB_LOSSY_HASH_TABLE_HPP
#define _KRB_LOSSY_HASH_TABLE_HPP

#include <inttypes.h>
#include <vector>
#include <krb/murmur_hash.hpp>

template <class Key, class Data, class HashFunc = murmur_hash_func<Key> >
class lossy_hash_table : public std::vector<Data>
{

protected:
  typedef std::vector<Data> base;

public:

  lossy_hash_table(uint32_t size)
    : base(size, Data()) {}

  Data & operator[](const Key &k);
  const Data & operator[](const Key &k) const;

protected:

  HashFunc hash_func;

};


//////////////////////////////////////////////////////////////////////
// implementation details
//////////////////////////////////////////////////////////////////////

template <class Key, class Data, class HashFunc>
Data & lossy_hash_table<Key, Data, HashFunc>::operator[](const Key &k)
{
  uint32_t H = hash_func(k) % base::size();
  return ((base *)this)->operator[](H);
}

template <class Key, class Data, class HashFunc>
const Data & lossy_hash_table<Key, Data, HashFunc>::operator[](const Key &k) const
{
  uint32_t H = hash_func(k) % base::size();
  return ((base *)this)->operator[](H);
}

#endif // _KRB_LOSSY_HASH_TABLE_HPP
