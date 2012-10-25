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
  Parser for apache combined log format entries.

  The format of the log is line based.  Each line has the following
  fields:

  host rfc931 username [date/time] "method url protocol" statuscode \
  bytes "referrer" "useragent"

  By default this uses the nonstandard (for CLF) date format from
  VoxCAST: "%Y-%m-%d %H:%M:%S".  It can also handle CLF date format:
  "%d/%b/%Y:%H:%M:%S %z".
*/

#ifndef _KRB_APACHE_LOG_ENTRY_HPP
#define _KRB_APACHE_LOG_ENTRY_HPP

#include <istream>
#include <sstream>
#include <string>
#include <time.h>
#include <inttypes.h>

class apache_log_entry
{
public:

  apache_log_entry(bool use_clf_dates = false)
    : clf_dates(use_clf_dates) {}

  const char * host() const { return m_host.c_str(); }
  const char * rfc931() const { return m_rfc931.c_str(); }
  const char * username() const { return m_username.c_str(); }
  time_t time() const { return m_time; }
  const char * method() const { return m_method.c_str(); }
  const char * url() const { return m_url.c_str(); }
  const char * protocol() const { return m_protocol.c_str(); }
  uint16_t status() const { return m_status; }
  uint32_t bytes() const { return m_bytes; }
  const char * referrer() const { return m_referrer.c_str(); }
  const char * user_agent() const { return m_user_agent.c_str(); }

protected:

  struct log_token : public std::string {};

  friend std::istream & operator>>(std::istream &is, log_token &tok);
  friend std::istream & operator>>(std::istream &is, apache_log_entry &e);

  log_token m_host;
  log_token m_rfc931;
  log_token m_username;
  time_t m_time;
  std::string m_method;
  std::string m_url;
  std::string m_protocol;
  uint16_t m_status;
  uint32_t m_bytes;
  log_token m_referrer;
  log_token m_user_agent;

  bool clf_dates;
};

// istream input operator
std::istream & operator>>(std::istream &is, apache_log_entry &e);

#endif // _KRB_APACHE_LOG_ENTRY_HPP
