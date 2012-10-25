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
  A generic templated resource pool.  This is thread safe.

  You can set:

    * low watermark: minimum number of resources in the pool
    * high watermark: maximum number of resources in the pool

  The pool attempts to be smart about the actual number of resources
  it keeps around.  It allocates more resources on demand (up to the
  high watermark), and deallocates resources when too many are unused.
  Resource allocation and deallocation is done according to a Policy,
  which can be specified as a template argument to the resource_pool
  class.  Several policies are provided (documented below):

    * basic_pool_policy
    * never_shrink_policy
    * fixed_growth_policy<N>
    * fixed_size_policy

  The pool _does not_ keep track of resources that have been reserved
  for use --- only ones available for use.  When a resource is
  returned to the pool, it _assumes_ the resource was taken from the
  pool.  This is a potential pitfall, but improves the pool's
  efficiency quite a bit.

  The pool defines a virtual protected "recycle" member function that
  by default does nothing.  Derived pool classes can implement recycle
  to perform any necessary re-initialization of resources that have
  just been returned to the pool, or just been added to the pool after
  a resize.
*/

#ifndef _KRB_RESOURCE_POOL_HPP
#define _KRB_RESOURCE_POOL_HPP

#include <inttypes.h>
#include <pthread.h>
#include <list>
#include <algorithm>
#include <krb/locker.hpp>


// a simple pool sizing policy.  grow() is called with the current
// size of the pool whenever we run out of available objects, and
// should return the number of _new_ objects to add to the pool.
// shrink() is called with the current pool size and number of free
// objects whenever an object is released, and should return the
// number of objects to _remove_ from the pool.  if you want to define
// your own pool sizing policy, it must support the same interface.
struct basic_pool_policy
{
  uint32_t grow(uint32_t cur_sz) const
  {
    // we'll use STL's approach: grow by a factor of 1.5
    return cur_sz / 2;
  }

  uint32_t shrink(uint32_t cur_sz, uint32_t open_sz) const
  {
    // release some resources if so if |open| / |pool| > 0.66, and try
    // to keep ~33% free
    if(open_sz <= 0.66 * cur_sz)
      return 0;
    else
      return open_sz - (uint32_t)(0.33 * cur_sz);
  }
};

// only grow, never shrink
struct never_shrink_policy : public basic_pool_policy
{
  uint32_t shrink(uint32_t cur_sz, uint32_t open_sz) const
  {
    return 0;
  }
};

// grow by a fixed amount every time (but stick with the standard
// shrinking policy)
template <uint32_t GrowthSize = 1>
struct fixed_growth_policy : public basic_pool_policy
{
  uint32_t grow(uint32_t cur_sz) const
  {
    return GrowthSize;
  }
};

// never resize, always just use the low watermark as the pool's size
struct fixed_size_policy
{
  uint32_t grow(uint32_t cur_sz) const { return 0; }
  uint32_t shrink(uint32_t cur_sz, uint32_t open_sz) const { return 0; }
};



// the generic resource pool!
template <class Resource, class Policy = basic_pool_policy>
class resource_pool
{
public:

  resource_pool(uint32_t low_watermark, uint32_t high_watermark);
  virtual ~resource_pool();

  // get a resource from the pool, or return NULL on failure (i.e.,
  // we've hit the high watermark)
  Resource * fetch();

  // release a resource back into the pool, and "recycle" it if
  // necessary
  void release(Resource *r);

  uint32_t allocated() const { return pool.size(); }
  uint32_t used() const { return allocated() - free(); }
  uint32_t free() const { return open.size(); }

protected:

  // perform any necessary actions on a newly released resource
  virtual void recycle(Resource *r) {}

  typedef std::list<Resource> resource_list;
  typedef typename std::list<Resource>::iterator resource_iterator;

  uint32_t wm_low, wm_high;
  resource_list pool;
  std::list<Resource *> open;
  pthread_mutex_t mutex;
  Policy policy;
};



//////////////////////////////////////////////////////////////////////
// implementation details
//////////////////////////////////////////////////////////////////////

template <class Resource, class Policy>
resource_pool<Resource, Policy>::resource_pool
  (uint32_t low_watermark, uint32_t high_watermark)
    : wm_low(low_watermark), wm_high(high_watermark),
      pool(0)
{
  pthread_mutex_init(&mutex, NULL);
}

template <class Resource, class Policy>
resource_pool<Resource, Policy>::~resource_pool()
{
  pthread_mutex_destroy(&mutex);
}

template <class Resource, class Policy>
Resource * resource_pool<Resource, Policy>::fetch()
{
  locker L(mutex);

  // any resources available?
  if(open.empty() && pool.size() >= wm_high)
    return NULL;

  if(open.empty()) {

    // we need to allocate more resources (we're not at our high
    // watermark yet).  ask the pool sizing policy how much we should
    // grow.

    uint32_t grow = pool.empty() ? wm_low :
      std::max(std::min(policy.grow(pool.size()),
                        (uint32_t)(wm_high - pool.size())),
               (uint32_t)1);

    resource_list add(grow);
    for(resource_iterator i = add.begin(); i != add.end(); ++i) {
      open.push_back(&(*i));
      recycle(open.back());
    }
    pool.splice(pool.end(), add);
  }

  // remove a resource from open, put it on closed, and return a
  // pointer to it
  Resource *r = open.back();
  open.pop_back();
  return r;
}

template <class Resource, class Policy>
void resource_pool<Resource, Policy>::release(Resource *r)
{
  locker L(mutex);

  // add the resource back into the free list
  recycle(r);
  open.push_back(r);

  // should we release some resources?
  if(pool.size() > wm_low) {

    uint32_t remove = policy.shrink(pool.size(), open.size());
    if(remove == 0)
      return;
    else if(pool.size() - remove < wm_low)
      remove = pool.size() - wm_low;

    // just iterate over resources in the pool, look for them in open,
    // and remove from both pool and open if found.  this is expensive
    // (O(n^2)) in this implementation, so we want to do it only
    // rarely.
    typename std::list<Resource *>::iterator oi;
    resource_iterator ri = pool.begin();
    for(; ri != pool.end() && remove; --remove) {

      for(oi = open.begin(); oi != open.end(); ++oi) {
        if(*oi == &(*ri)) {
          open.erase(oi);
          break;
        }
      }

      // if we found this resource in open, clobber it
      if(oi != open.end())
        ri = pool.erase(ri);
      else
        ++ri;
    }
  }
}

#endif // _KRB_RESOURCE_POOL_HPP
