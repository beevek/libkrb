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

#include <krb/apache_log_entry.hpp>

// read a "token" 
std::istream & operator>>(std::istream &is, apache_log_entry::log_token &tok)
{
  char c, start = 0;

  tok.clear();

  while(is.good()) {

    is.get(c);

    if(!start) {
      if(!isspace(c))
        start = c;
      if(c == '[' || c == '"')
        continue;

    } else if((start == '[' && c == ']') || (start == '"' && c == '"') ||
              (isspace(c) && start != '[' && start != '"'))
      return is;

    tok += c;

  }

  return is;
}

std::istream & operator>>(std::istream &is, apache_log_entry &e)
{
  apache_log_entry::log_token tmp_datetime, tmp_request;
  struct tm ts;

  is >> e.m_host >> e.m_rfc931 >> e.m_username
     >> tmp_datetime >> tmp_request
     >> e.m_status >> e.m_bytes
     >> e.m_referrer >> e.m_user_agent;

  std::istringstream req(tmp_request);
  req >> e.m_method >> e.m_url >> e.m_protocol;

  const char *format = e.clf_dates ?
    "%d/%b/%Y:%H:%M:%S %z" : "%Y-%m-%d %H:%M:%S";

  if(strptime(tmp_datetime.c_str(), format, &ts))
    e.m_time = mktime(&ts);
  else {
    e.m_time = 0;
    is.setstate(is.failbit);
  }

  return is;
}
