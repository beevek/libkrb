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
  A couple simple and useful exception classes.
*/

#ifndef _KRB_EXCEPTIONS_HPP
#define _KRB_EXCEPTIONS_HPP

#include <string.h>
#include <string>

class string_exception
{
public:

  string_exception() : m_reason("Unspecified") {}
  string_exception(std::string r) : m_reason(r) {}

  const std::string & reason() const { return m_reason; }

protected:
  std::string m_reason;

};

class strerror_exception : public string_exception
{
public:

  strerror_exception() : string_exception() {}
  strerror_exception(int e) : string_exception(strerror(e)) {}
  strerror_exception(std::string r, int e)
    : string_exception(r + strerror(e)) {}
};

#endif // _KRB_EXCEPTIONS_HPP
