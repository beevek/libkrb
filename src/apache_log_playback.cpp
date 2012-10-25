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

#include <krb/apache_log_playback.hpp>

apache_log_playback::apache_log_playback
  (std::istream &input, apache_log_callback &callback,
   uint32_t buffered_entries, double speed)
    : is(input), cb(callback),
      buf_size(buffered_entries), speed_mult(speed),
      line_no(0), last_entry_time(0), delay_usec(0)
{
}

bool apache_log_playback::single_entry()
{
  apache_log_entry e;
  bool read_entry = false, keep_reading = false;

  do {

    // first, try to read in an entry
    if(is.good()) {
      is >> e;
      read_entry = true;

      if(is.fail())
        return false;
    }

    if(buf_size > 0) {
      // add the entry to the buffer if we got a new one
      if(read_entry)
        buffer.insert(buf_inserter(e.time(), e));

      // if the buffer is full, pop an entry off for processing; do
      // the same if nothing was read so we drain the buffer; and if
      // the buffer isn't full and we read an entry, keep reading
      if(buffer.size() >= buf_size || !read_entry) {
        e = buffer.begin()->second;
        buffer.erase(buffer.begin());
        keep_reading = false;
      } else if(buffer.size() < buf_size && read_entry)
        keep_reading = true;

    }

  } while(keep_reading);


  // if there was a previous entry and we're adding delay, compute
  // delay_usec, then sleep
  if(speed_mult > 0 && last_entry_time > 0) {

    delay_usec = 1000000 * (e.time() - last_entry_time) / speed_mult;

    if(delay_usec < 0)
      delay_usec = 0; // in case of out of order

    if(delay_usec > 0)
      usleep(delay_usec);
  }

  if(speed_mult > 0) {
    if(e.time() > last_entry_time)
      last_entry_time = e.time();
  }

  ++line_no; // only increment this when we process an entry

  // call the callback and return its return value
  return cb(*this, e);
}

void apache_log_playback::all_entries()
{
  while(single_entry()) {}
}
