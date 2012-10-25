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
  Your standard mutex-locking objects.  After the object is
  constructed, the lock has been acquired.  When the object goes out
  of context, the lock is released.
*/

#ifndef _KRB_LOCKER_HPP
#define _KRB_LOCKER_HPP

#include <pthread.h>
#include <errno.h>
#include <krb/exceptions.hpp>

class locker
{
public:

  locker(pthread_mutex_t &mutex)
    : M(mutex)
  {
    int rv = pthread_mutex_lock(&M);
    if(rv != 0)
      throw strerror_exception("Failed acquiring lock", rv);
  }

  ~locker()
  {
    pthread_mutex_unlock(&M);
  }

protected:
  pthread_mutex_t &M;

};


class write_locker
{
public:

  write_locker(pthread_rwlock_t &lock)
    : L(lock)
  {
    int rv = pthread_rwlock_wrlock(&L);
    if(rv != 0)
      throw strerror_exception("Failed acquiring wrlock", rv);
  }

  ~write_locker()
  {
    pthread_rwlock_unlock(&L);
  }

protected:
  pthread_rwlock_t &L;

};


class read_locker
{
public:

  read_locker(pthread_rwlock_t &lock)
    : L(lock)
  {
    int rv = pthread_rwlock_rdlock(&L);
    if(rv != 0)
      throw strerror_exception("Failed acquiring rdlock", rv);
  }

  ~read_locker()
  {
    pthread_rwlock_unlock(&L);
  }

  void upgrade_to_write()
  {
    pthread_rwlock_unlock(&L);
    int rv = pthread_rwlock_wrlock(&L);
    if(rv != 0)
      throw strerror_exception("Failed upgrading to wrlock", rv);
  }

protected:
  pthread_rwlock_t &L;

};


class try_write_locker
{
public:

  try_write_locker(pthread_rwlock_t &lock)
    : L(lock), ok(false)
  {
    int rv = pthread_rwlock_trywrlock(&L);
    if(rv != 0 && rv != EBUSY)
      throw strerror_exception("Failed trying wrlock", rv);
    ok = (rv == 0);
  }

  ~try_write_locker()
  {
    if(ok)
      pthread_rwlock_unlock(&L);
  }

  bool is_locked() const { return ok; }

protected:
  pthread_rwlock_t &L;
  bool ok;

};


class try_read_locker
{
public:

  try_read_locker(pthread_rwlock_t &lock)
    : L(lock), ok(false)
  {
    int rv = pthread_rwlock_tryrdlock(&L);
    if(rv != 0 && rv != EBUSY)
      throw strerror_exception("Failed trying rdlock", rv);
    ok = (rv == 0);
  }

  ~try_read_locker()
  {
    if(ok)
      pthread_rwlock_unlock(&L);
  }

  bool is_locked() const { return ok; }

protected:
  pthread_rwlock_t &L;
  bool ok;

};

#endif // _KRB_LOCKER_HPP
