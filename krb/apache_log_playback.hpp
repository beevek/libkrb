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
  Buffered apache log playback.  This executes a user-defined callback
  (function object) for every log entry.

  If desired, it buffers some amount of log entries in an attempt to
  sort them by time so the requests actually happen in the same order
  as they did in practice.  This is necessary because entries need not
  be inserted into the logs in chronological order of request time,
  but adds memory and computational overhead so is optional.

  Also optionally, it inserts delay so that the log playback is some
  fraction of realtime, at least with respect to request time.  By
  setting speed = 1, you can get exactly realtime; speed = 2, 2x
  realtime, etc.  The exception is speed = 0, which will cause the
  playback to ignore request times altogether and pump entries as fast
  as possible.

  The callback function object must be derived from
  apache_log_callback and define the following member function:

    bool operator()
      (const apache_log_playback &p,
       const apache_log_entry &e);

  If the function returns false, log playback is terminated.
*/

#ifndef _KRB_APACHE_LOG_PLAYBACK_HPP
#define _KRB_APACHE_LOG_PLAYBACK_HPP

#include <istream>
#include <map>
#include <krb/apache_log_entry.hpp>

class apache_log_playback; // forward declaration

struct apache_log_callback
{
  virtual ~apache_log_callback() {}
  virtual bool operator()
    (const apache_log_playback &p,
     const apache_log_entry &e) = 0;
};

class apache_log_playback
{
public:

  apache_log_playback
    (std::istream &input, apache_log_callback &callback,
     uint32_t buffered_entries = 0, double speed = 0.0);

  // read a single log entry and call the callback; returns false if
  // the log is finished or there is some kind of failure.  may sleep
  // for a while before calling the callback if in realtime mode.
  bool single_entry();

  // read all the log entries and call the callback for each
  void all_entries();

  // get the current log line number
  uint32_t line() const { return line_no; }

protected:

  std::istream &is;
  apache_log_callback &cb;

  uint32_t buf_size;
  double speed_mult;

  typedef std::multimap<time_t, apache_log_entry> buf_type;
  typedef std::pair<time_t, apache_log_entry> buf_inserter;
  typedef buf_type::iterator buf_iterator;

  buf_type buffer;
  uint32_t line_no;
  time_t last_entry_time;
  int32_t delay_usec;
};

#endif // _KRB_APACHE_LOG_PLAYBACK_HPP
