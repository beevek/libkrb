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
  A simple templated LRU cache.  This is not thread safe.
*/

#ifndef _KRB_LRU_CACHE_HPP
#define _KRB_LRU_CACHE_HPP

#include <inttypes.h>
#include <list>
#include <tr1/unordered_map>


template <class Key, class Data>
class lru_cache_t
{
protected:

  struct lru_item_t
  {
    Key key;
    Data value;
    uint32_t size;
    lru_item_t(const Key &k, const Data &v, uint32_t sz)
      : key(k), value(v), size(sz) {}
  };

  typedef std::list<lru_item_t> lru_list_t;
  typedef std::tr1::unordered_map<Key, typename lru_list_t::iterator> key_hash_t;

  void access(typename lru_list_t::iterator li);

public:

  lru_cache_t(uint32_t size_limit)
    : max_size(size_limit), cur_size(0),
      cache_hits(0), cache_misses(0) {}

  // find a key in the cache and return a (non-const) pointer to its
  // data (i.e., you can modify the cached data if you want).  if the
  // item is not found, returns NULL.  if the item is found, the item
  // is moved to the back of the LRU queue (it's marked most recently
  // used).  XXX: for thread-safe, return a shared_ptr<Data> in case
  // LRU discards the item in one thread while we're operating on it
  // in another.
  Data * lookup(const Key &k);

  // insert a key/value pair in the cache.  if the key is already
  // cached, replaces its value.  in either case, the new/replaced
  // item is marked most recently used.  pass the optional sz to give
  // this item a specific "size"; by default we just use one and the
  // size limit is a limit on the number of objects in the cache.
  void insert(const Key &k, const Data &value, uint32_t sz = 1);

  // delete a key/value pair from the cache.  if it doesn't exist,
  // returns false.
  bool purge(const Key &k);

  // empty the cache
  void clear();

  // shrink or grow the available cache space
  void resize(uint32_t size_limit);

  // statistics
  uint32_t size() const { return cur_size; }
  uint32_t size_limit() const { return max_size; }
  uint64_t hits() const { return cache_hits; }
  uint64_t misses() const { return cache_misses; }
  double ratio() const { return double(hits()) / double(hits() + misses()); }

protected:

  lru_list_t lru_list;
  key_hash_t key_hash;

  uint32_t max_size, cur_size;
  uint64_t cache_hits, cache_misses;

};


///// implementation details

// move li to the MRU spot
template <class Key, class Data>
void lru_cache_t<Key, Data>::access(typename lru_list_t::iterator li)
{
  lru_list.splice(lru_list.begin(), lru_list, li);
}

template <class Key, class Data>
Data * lru_cache_t<Key, Data>::lookup(const Key &k)
{
  typename lru_cache_t<Key, Data>::key_hash_t::iterator ki =
    key_hash.find(k);

  if(ki != key_hash.end()) { // found!
    access(ki->second);
    ++cache_hits;
    return &(ki->second->value);

  } else {
    ++cache_misses;
    return NULL;
  }
}

template <class Key, class Data>
void lru_cache_t<Key, Data>::insert
  (const Key &k, const Data &value, uint32_t sz)
{
  typename lru_cache_t<Key, Data>::key_hash_t::iterator ki =
    key_hash.find(k);

  if(ki != key_hash.end()) {
    // key exists, move it to the MRU spot and update its value, and
    // the size of the cache
    access(ki->second);
    ki->second->value = value;
    cur_size = cur_size - ki->second->size + sz;
    ki->second->size = sz;

  } else {

    // insert the item
    lru_list.push_front(lru_item_t(k, value, sz));
    cur_size += sz;
    key_hash[k] = lru_list.begin();

    // discard LRU items until the cache's size is within our limit
    while(cur_size > max_size) {
      typename lru_cache_t<Key, Data>::lru_list_t::iterator li_last =
        lru_list.end();
      --li_last;

      key_hash.erase(li_last->key);
      cur_size -= li_last->size;
      lru_list.erase(li_last);
    }

  }
}

template <class Key, class Data>
bool lru_cache_t<Key, Data>::purge(const Key &k)
{
  typename lru_cache_t<Key, Data>::key_hash_t::iterator ki =
    key_hash.find(k);

  if(ki != key_hash.end()) {
    cur_size -= ki->second->size;
    lru_list.erase(ki->second);
    key_hash.erase(ki);
    return true;
  } else
    return false;
}

template <class Key, class Data>
void lru_cache_t<Key, Data>::clear()
{
  key_hash.clear();
  lru_list.clear();
  cur_size = 0;
}

template <class Key, class Data>
void lru_cache_t<Key, Data>::resize(uint32_t size_limit)
{
  max_size = size_limit;
}


#endif // _KRB_LRU_CACHE_HPP
