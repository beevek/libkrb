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

#include <krb/config_file_parser.hpp>
#include <krb/exceptions.hpp>
#include <fstream>
#include <sstream>
#include <boost/spirit.hpp>

namespace bs = boost::spirit;

typedef bs::file_iterator<char> iterator_t;
typedef bs::scanner<iterator_t> scanner_t;
typedef bs::rule<scanner_t> rule_t;

bool real_parse(const char *path, rule_t &rule);


template <class Iterator>
struct config_push_group
{
  config_push_group
    (config_file_parser &C, std::stack<std::string> &strstack)
      : C(C), strstack(strstack) {}

  void operator()(Iterator start, Iterator end) const
  {
    strstack.push(std::string());
    C.push_group(std::string(start, end));
  }

  config_file_parser &C;
  std::stack<std::string> &strstack;
};

template <class Iterator>
struct config_parse_group
{
  config_parse_group
    (config_file_parser &C, std::stack<std::string> &strstack)
      : C(C), strstack(strstack) {}

  void operator()(Iterator start, Iterator end) const
  {
    C.parse_group(strstack.top());
    strstack.pop();
  }

  config_file_parser &C;
  std::stack<std::string> &strstack;
};

template <class Iterator>
struct config_string_append
{
  config_string_append(std::stack<std::string> &strstack)
    : strstack(strstack) {}

  void operator()(Iterator start, Iterator end) const
  {
    strstack.top().append(start, end);
  }

  std::stack<std::string> &strstack;
};

config_file_parser::~config_file_parser()
{
  group_callback_map::iterator gci = groups.begin();
  for(; gci != groups.end(); ++gci) {
    while(!gci->second.empty()) {
      delete gci->second.top();
      gci->second.pop();
    }
  }
}

void config_file_parser::push_group_callback
  (const std::string &name, config_group_callback *cb)
{
  groups[name].push(cb);
}

void config_file_parser::pop_group_callback
  (const std::string &name)
{
  group_callback_map::iterator gci = groups.find(name);
  if(gci == groups.end() || gci->second.empty())
    return;

  delete gci->second.top();
  gci->second.pop();
}

void config_file_parser::push_group(const std::string &str)
{
  // push a group type onto the stack, so that we parse the topmost
  // group on the stack in parse_group, and call the group entry
  // callback for this type of group.

  // push this group type onto the stack
  group_stack.push(str);

  group_callback_map::iterator cbi = groups.find(str);
  if(cbi == groups.end())
    return; // silently refuse to parse an unknown group type

  // call the group entry callback
  cbi->second.top()->entry(*this);
}

void config_file_parser::parse_group(const std::string &str)
{
  // call the current group callback for the topmost group type on the
  // stack, with the given string converted into a strstream.  then,
  // pop the group off the stack.

  if(group_stack.empty())
    return; // should never happen

  group_callback_map::iterator cbi = groups.find(group_stack.top());
  if(cbi != groups.end()) {
    // convert str into a stringstream
    std::stringstream ss(str, std::stringstream::in);

    // call the group exit callback, with the group's contents
    cbi->second.top()->exit(*this, ss);
  }

  // pop this group off the stack
  group_stack.pop();
}

bool config_file_parser::parse(const char *path)
{
  // stack, with the top containing the string of assignments for the
  // current group
  std::stack<std::string> strstack;

  // actors
  config_push_group<iterator_t> g_push(*this, strstack);
  config_parse_group<iterator_t> g_parse(*this, strstack);
  config_string_append<iterator_t> g_append(strstack);

  // set up our EBNF rules and call the parser
  rule_t ws, var_name, assignment, group, config_file;
  ws = *(bs::space_p | bs::comment_p("#"));
  var_name = (bs::alpha_p | bs::ch_p('_'))
    >> *(bs::alnum_p | bs::ch_p('_') | bs::ch_p('.'));
  assignment = ws
    >> (var_name >> ws
        >> bs::ch_p('=') >> ws
        >> *(bs::anychar_p - bs::eol_p)
        >> bs::eol_p) [g_append];
  group = ws
    >> var_name [g_push] >> ws
    >> bs::ch_p('{') >> ws
    >> *(assignment | (group [g_parse])) >> ws
    >> bs::ch_p('}')
    >> ws;
  config_file = *(group [g_parse]);

  return real_parse(path, config_file);
}

bool real_parse(const char *path, rule_t &rule)
{
  // open the file in a spirit-compatible way
  iterator_t first(path);
  if(!first)
    return false;
  iterator_t last = first.make_end();

  return bs::parse(first, last, rule).full;
}
