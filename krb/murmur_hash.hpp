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
  Murmur hash function, based on Austin Appleby's code from
  http://murmurhash.googlepages.com.  Murmur is a very fast, very
  well distributed hash function.
*/

#ifndef _KRB_MURMUR_HASH_HPP
#define _KRB_MURMUR_HASH_HPP

#include <inttypes.h>
#include <string.h>

class murmur_hash
{
public:
  uint32_t operator()(const void *key, uint32_t sz, uint32_t seed) const;
};

template <class Key>
struct murmur_hash_func : protected murmur_hash
{
  uint32_t operator()(Key v) const;
};


//////////////////////////////////////////////////////////////////////
// implementation details
//////////////////////////////////////////////////////////////////////

template <class Key>
uint32_t murmur_hash_func<Key>::operator()(Key v) const
{
  return this->operator()(&v, sizeof(v), 0);
}

// string specializations

template <>
struct murmur_hash_func<const char *> : protected murmur_hash
{
  uint32_t operator()(const char *v) const
  {
    return murmur_hash::operator()(v, strlen(v), 0);
  }
};

template <>
struct murmur_hash_func<char *> : protected murmur_hash
{
  uint32_t operator()(char *v) const
  {
    return murmur_hash::operator()(v, strlen(v), 0);
  }
};

#endif // _KRB_MURMUR_HASH_HPP
