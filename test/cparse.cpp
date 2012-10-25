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
#include <iostream>
#include <vector>
#include <stdio.h>
#include <boost/program_options.hpp>

namespace po = boost::program_options;

// we'd like to fill in this structure from a config file
struct vhost
{
  vhost(int N) : N(N), locations(0) {}
  int N;
  int locations; // count the number of "location {}" groups
  int device_id;
  std::string server_name;
};

// we'd like to read in a whole bunch of said structures
std::vector<vhost> vhosts;


// how to read in a "location {}" group --- this just ignores the
// contents and increments a counter in the associated vhost
struct location_callback : public config_group_callback
{
  location_callback(vhost &V) : V(V) {}
  bool entry(config_file_parser &CF) { return true; }
  bool exit(config_file_parser &CF, std::istream &is)
  {
    ++V.locations;
    return true;
  }
  vhost &V;
};

// how to read in a "vhost {}" group --- this creates a new vhost
// object; pushes onto the callback stack for "location" a function
// object that knows about the current vhost object; uses
// program_options to parse the contents of the group and fill in the
// values in the current vhost; prints out some details; and finally,
// pops the "location" callback.
struct vhost_callback : public config_group_callback
{
  static int N;
  int my_N;
  int V;

  bool entry(config_file_parser &CF)
  {
    my_N = ++N;
    printf("entered vhost #%d\n", my_N);

    vhosts.push_back(vhost(my_N));
    V = vhosts.size() - 1;

    CF.push_group_callback("location", new location_callback(vhosts.back()));

    return true;
  }

  bool exit(config_file_parser &CF, std::istream &is)
  {
    printf("exited vhost #%d with %d locations\n", my_N, vhosts[V].locations);

    po::options_description vhost_opt;
    vhost_opt.add_options()
      ("device_id", po::value<int>(&vhosts[V].device_id))
      ("server_name", po::value<std::string>(&vhosts[V].server_name));
    po::variables_map vm;
    po::store(po::parse_config_file(is, vhost_opt), vm, true);
    po::notify(vm);

    printf("device_id is %d, server_name is %s\n",
           vhosts[V].device_id,
           vhosts[V].server_name.c_str());

    CF.pop_group_callback("location");

    return true;
  }
};

int vhost_callback::N = 0;

int main(int argc, char **argv)
{
  if(argc < 2) {
    printf("Usage: %s <cfg file>\n", argv[0]);
    return 1;
  }

  // the only top-level group type we know about is "vhost" in this
  // example.  push a callback onto the stack.
  config_file_parser CF;
  CF.push_group_callback("vhost", new vhost_callback);

  if(!CF.parse(argv[1]))
    printf("failed to parse %s\n", argv[1]);

  return 0;
}
