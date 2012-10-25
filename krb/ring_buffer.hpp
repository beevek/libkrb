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
  A basic templated ring buffer class.  You can do the usual ring
  buffer read and write, and in addition, peek (read without advancing
  the ring start), and write directly into the buffer (after which
  you're required to advance the buffer's end counter yourself by
  calling write_advance).
*/

#ifndef _KRB_RING_BUFFER_HPP
#define _KRB_RING_BUFFER_HPP

#include <inttypes.h>
#include <string.h>

template <class T>
class ring_buffer
{
public:
  ring_buffer(uint32_t size);
  ~ring_buffer();

  bool read(T *out, uint32_t n);
  bool peek(T *out, uint32_t n);
  bool read_advance(uint32_t n);

  T * write_direct_access() { return end; }
  bool write(const T *in, uint32_t n);
  bool write_advance(uint32_t n);

  uint32_t used() const { return count; }
  uint32_t available() const { return buf_sz - count; }
  uint32_t available_contiguous() const;
  bool full() const { return (start == end && count > 0); }

protected:
  T *buf;
  T *start, *end, *last;
  uint32_t count, buf_sz;
};

template <class T>
ring_buffer<T>::ring_buffer(uint32_t size)
  : buf(new T[size]), start(buf), end(buf), last(buf + size*sizeof(T)),
    count(0), buf_sz(size)
{
}

template <class T>
ring_buffer<T>::~ring_buffer()
{
  delete [] buf;
}

template <class T>
uint32_t ring_buffer<T>::available_contiguous() const
{
  if(start < end || (start == end && count != buf_sz))
    // space available from <end> to the end of the buffer
    return (last - end) / sizeof(T);
  else if(start > end)
    // space available from <end> to <start>
    return (start - end) / sizeof(T);
  else
    return 0;
}

template <class T>
bool ring_buffer<T>::read_advance(uint32_t n)
{
  if(n > count)
    return false;

  if(start < end)
    start += n*sizeof(T);
  else {
    const uint32_t end_chunk_size = (last - start) / sizeof(T);
    if(end_chunk_size > n)
      start += n*sizeof(T);
    else if(end_chunk_size < n)
      start = buf + (n-end_chunk_size)*sizeof(T);
    else
      start = buf;
  }

  if(start == last)
    start = buf;

  count -= n;

  return true;
}

template <class T>
bool ring_buffer<T>::write_advance(uint32_t n)
{
  if(n > available())
    return false;

  if(start <= end) {
    const uint32_t end_chunk_size = (last - end) / sizeof(T);
    if(end_chunk_size > n)
      end += n*sizeof(T);
    else if(end_chunk_size < n)
      end = buf + (n-end_chunk_size)*sizeof(T);
    else
      end = buf;
  } else
    end += n*sizeof(T);

  if(end == last)
    end = buf;

  count += n;

  return true;
}

template <class T>
bool ring_buffer<T>::peek(T *out, uint32_t n)
{
  if(n > count)
    return false;

  if(start < end)
    memcpy(out, start, n*sizeof(T));
  else {
    const uint32_t end_chunk_size = (last - start) / sizeof(T);
    if(end_chunk_size >= n)
      memcpy(out, start, n*sizeof(T));
    else {
      memcpy(out, start, end_chunk_size*sizeof(T));
      memcpy(out + end_chunk_size*sizeof(T), buf, (n-end_chunk_size)*sizeof(T));
    }
  }

  return true;
}

template <class T>
bool ring_buffer<T>::read(T *out, uint32_t n)
{
  return (peek(out, n) && read_advance(n));
}

template <class T>
bool ring_buffer<T>::write(const T *in, uint32_t n)
{
  if(n > available())
    return false;

  if(start <= end) {
    const uint32_t end_chunk_size = (last - end) / sizeof(T);
    if(end_chunk_size >= n)
      memcpy(end, in, n*sizeof(T));
    else {
      memcpy(end, in, end_chunk_size*sizeof(T));
      memcpy(buf, in + end_chunk_size*sizeof(T), (n-end_chunk_size)*sizeof(T));
    }
  } else
    memcpy(end, in, n*sizeof(T));

  return write_advance(n);
}

#endif // _KRB_RING_BUFFER_HPP
