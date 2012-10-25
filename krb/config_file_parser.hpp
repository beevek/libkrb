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
  A parser for simple "grouped" configuration files that look like
  this:

    item {
      # comment
      ...stuff...
      foo {
        ...stuff...
      }
    }

    thing {
      ...stuff...
    }

    thing {
      ...stuff...
    }

  Each "group" is of a particular "type" (e.g., "item", "foo", "thing"
  in the example).  The caller associates (pointers to) callback
  function objects with different group types.  These function objects
  are called upon "entry" into a group; and then again upon "exit"
  from the group, with the contents of the group (minus subgroups) as
  an istream.

  My recommendation is to use boost's program_options to parse the
  resulting configuration istream as a config file, which will handle
  syntax like:

    name = value
    name = "longer value with spaces"

  and takes care of type conversions, etc.  It's very easy to specify
  a set of options with program_options.  In fact, this parser
  enforces line-based assignments of the form "name = value".
  However, rather that create a symbol table within this parser, I
  deemed the extra functionality of program_options nice enough to
  just rely on that for parsing name/value pairs.

  Callbacks are kept in a stack.  To set the callback for a group type
  within the current context (i.e., within some super-group), call
  push_group_callback() in the super-group's entry() callback; and
  call pop_group_callback() in the super-group's exit() callback.  For
  example, you might want something like:

    vhost {
      location {...}
      location {...}
      ...
    }

  Then, in the entry() callback for vhost, call
  C.push_group_callback("location", new location_callback) where
  location_callback is aware of its ownership by the vhost object.
  And in the exit() callback for vhost, call
  C.pop_group_callback("location").

  On destruction, config_file_parser deletes all callback function
  objects in its stacks; i.e., it is assumed you make calls like:
  C.push_group_callback("groupname", new group_callback).
*/

#ifndef _KRB_CONFIG_FILE_PARSER_HPP
#define _KRB_CONFIG_FILE_PARSER_HPP

#include <string>
#include <map>
#include <stack>
#include <istream>


// forward declarations
class config_file_parser;
template <class Iterator> struct config_push_group;
template <class Iterator> struct config_parse_group;


// callbacks must derive from this abstract class
struct config_group_callback
{
  virtual bool entry(config_file_parser &) = 0;
  virtual bool exit(config_file_parser &, std::istream &) = 0;
};


class config_file_parser
{
public:

  ~config_file_parser();

  void push_group_callback
    (const std::string &name, config_group_callback *cb);
  void pop_group_callback(const std::string &name);

  // parse the config file at the given path
  bool parse(const char *path);

protected:

  typedef std::map
    <std::string,
     std::stack<config_group_callback *> >
    group_callback_map;

  group_callback_map groups;
  std::stack<std::string> group_stack;

  void push_group(const std::string &str);
  void parse_group(const std::string &str);
  template <class Iterator> friend struct config_push_group;
  template <class Iterator> friend struct config_parse_group;

};

#endif // _CONFIG_FILE_PARSER_HPP
