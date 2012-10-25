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
  Simple level compressed trie (LC-Trie), which only allows set
  membership queries.  This class is mainly meant for use with IP
  prefix lists.  Template parameter IPType will generally be one of
  the types defined here: ipv4 or ipv6.

  This algorithm is implemented as described in: S. Nilsson and
  G. Karlsson.  Fast address lookup for Internet routers.  In
  Proc. IFIP 4th International Conference on Broadband Communications,
  pp 11-22, 1998.  This code is based in part on S. Nilsson's code at
  http://www.csc.kth.se/~snilsson/software/router/C/

  Each LC trie is able to store up to 512K prefixes.
*/

#ifndef _KRB_LC_TRIE
#define _KRB_LC_TRIE

#include <inttypes.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <vector>
#include <fstream>
#include <sstream>
#include <typeinfo>
#include <boost/serialization/string.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/filter/gzip.hpp>


/* first we'll define types and operators for IPv4 and IPv6 */

typedef uint32_t ipv4;
struct ipv6
{
  uint64_t hi;
  uint64_t lo;

private:
  friend class boost::serialization::access;
  template <class Archive>
  void serialize(Archive &ar, const unsigned int version)
  {
    ar & hi;
    ar & lo;
  }
};


// various LC-trie related operators for ipv4 and ipv6, all
// implemented below

template <class IPType>
bool strtoip(const char *str, IPType *out);

ipv4 EXTRACT(int p, int n, ipv4 str);
ipv4 REMOVE(int p, ipv4 str);

bool operator<(const ipv6 &a, const ipv6 &b);
bool operator>(const ipv6 &a, const ipv6 &b);
bool operator==(const ipv6 &a, const ipv6 &b);
bool operator==(const ipv6 &ip, uint32_t i);
ipv6 operator^(const ipv6 &a, const ipv6 &b);
uint32_t operator+(uint32_t i, const ipv6 &ip);
ipv6 EXTRACT(int p, int n, ipv6 str);
ipv6 REMOVE(int p, ipv6 str);


/* next up, the lc_trie templated class */

template <class IPType, uint32_t adrsize = 8*sizeof(IPType)>
class lc_trie
{
protected:

  // trie node
  typedef uint32_t node_t;

  // methods to get/set chunks of bits from a 32-bit node.  we use the
  // first 5 bits for the branching factor; the next 7 bits for the
  // skip value; and the last 20 bits for the address.  this allows us
  // to store up to 512k prefixes.

  node_t SETBRANCH(node_t branch) { return branch << 27; }
  node_t GETBRANCH(node_t n) const { return n >> 27; }
  node_t SETSKIP(node_t skip) { return skip << 20; }
  node_t GETSKIP(node_t n) const { return n >> 20 & 127; }
  node_t SETADR(node_t adr) { return adr; }
  node_t GETADR(node_t n) const { return n & 1048575; }


  // base vector
  struct base_t
  {
    IPType str;    // the routing entry
    int len;       // its length

  private:
    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive &ar, const unsigned int version)
    {
      ar & str;
      ar & len;
    }
  };


  // core LC-trie data structure: a trie and a base vector.  we do not
  // need a prefix vector as in the general LC-trie implementation, as
  // stated above.

  // main trie search structure
  std::vector<node_t> trie;

  // base vector containing the actual entry strings
  std::vector<base_t> base;

  // compilation parameters
  double comp_fill_factor;
  int comp_root_branching_factor;

  // cached statistics string
  std::string cached_stats;

  // serialization method for boost's serialization library; save/load
  // the trie and base vector to a serialized archival format, e.g.,
  // for storage/loading from disk
  friend class boost::serialization::access;
  template <class Archive>
  void serialize(Archive &ar, const unsigned int version)
  {
    ar & trie;
    ar & base;
    ar & comp_fill_factor;
    ar & comp_root_branching_factor;
    ar & cached_stats;
  }

public:

  // a string for insertion into our trie consists of an IP, followed
  // by the length (in bits) of the prefix represented by that string.
  // for example, ipv4 123.456.0.0/16 becomes the uint32_t
  // representation of 123.456.0.0, and the length is 16 bits.
  typedef base_t input_string_t;

protected:

  // compare two input strings
  struct comparator_t
  {
    int strcmp(const input_string_t &a, const input_string_t &b) const;
    bool operator()(const input_string_t &a, const input_string_t &b) const;
  };

  // is a a prefix of b?
  bool isprefix(input_string_t &a, input_string_t &b) const;

  // compute the branch and skip values for the root of the tree that
  // covers the base array from position 'first' to 'first+n+1'.
  // disregard the first 'prefix' characters.  assumptions:
  //   1. n >= 2
  //   2. base[first] != base[first+n-1]
  void compute_branch
    (std::vector<base_t> &base,
     int prefix, int first, int n,
     int *branch, int *newprefix) const;

  // recursively build a tree that covers the base array from position
  // 'first' to 'first+n-1'.  disregard the first 'prefix' characters
  // of the strings.  'pos' is the position for the root of this tree,
  // and 'nextfree' is the first position in the trie vector that
  // hasn't yet been reserved.
  void build_recursive
    (std::vector<node_t> &tree,
     std::vector<base_t> &base,
     int prefix, int first, int n, int pos, int *nextfree);

  void traverse
    (const std::vector<node_t> &t,
     node_t r,
     int depth, int *totdepth, int *maxdepth) const;

public:

  lc_trie(double fill_factor = 0.5, int root_branching_factor = 0)
    : comp_fill_factor(fill_factor),
      comp_root_branching_factor(root_branching_factor) {}

  // compile the LC-trie from a vector of input strings (IP addresses
  // + prefix lengths in bits); note that the input vector will be
  // modified
  bool build(std::vector<input_string_t> &strings);

  // search for ip in the trie; returns true if ip falls within one of
  // the prefixes represented by the trie
  bool search(const IPType &ip) const;

  // methods to save and load compiled LC-tries to binary files using
  // boost's serialization library
  bool save(const char *filename) const;
  bool load(const char *filename);

  // return some stats about the trie in a string
  void stats(std::string &out);

};


/* functions to load plaintext files of CIDR-formatted prefixes into
   ipv4 or ipv6 LC-tries.  the files must have one prefix per line, in
   the following format:

   ipv4: iii.iii.iii.iii/cidr (e.g., 123.456.0.0/16)
   ipv6: aaaa:bbbb:cccc:dddd:eeee:ffff:gggg:hhhh/cidr
       or
         aaaa:...::.../cidr where :: represents zeros
       (e.g., 2001:4c40:1::/48 or
       2001:1598:1:6667:a00:20ff:fec0::/112)

   the CIDR bits may be left off in which case the prefix is treated
   as a /32 (ipv4) or /128 (ipv6).  basically anything handled by
   inet_pton is acceptable.

   avoid whitespace or any other characters in the file.  there are no
   comment characters.
 */
template <class IPType, uint32_t adrsize>
bool compile_lc_trie(const char *filename, lc_trie<IPType, adrsize> &trie);



//////////////////////////////////////////////////////////////////////
// implementation details: lc_trie class
//////////////////////////////////////////////////////////////////////


template <class IPType, uint32_t adrsize>
int lc_trie<IPType, adrsize>::comparator_t::strcmp
  (const lc_trie<IPType, adrsize>::input_string_t &a,
   const lc_trie<IPType, adrsize>::input_string_t &b) const
{
  if(a.str < b.str)
    return -1;
  else if(a.str > b.str)
    return 1;
  else if(a.len < b.len)
    return -1;
  else if(a.len > b.len)
    return 1;
  else
    return 0;
}


template <class IPType, uint32_t adrsize>
bool lc_trie<IPType, adrsize>::comparator_t::operator()
  (const lc_trie<IPType, adrsize>::input_string_t &a,
   const lc_trie<IPType, adrsize>::input_string_t &b) const
{
  return (strcmp(a, b) < 0);
}


template <class IPType, uint32_t adrsize>
bool lc_trie<IPType, adrsize>::isprefix
  (lc_trie<IPType, adrsize>::input_string_t &a,
   lc_trie<IPType, adrsize>::input_string_t &b) const
{
  return (a.len == 0 ||
          (a.len <= b.len &&
           EXTRACT(0, a.len, a.str) ==
           EXTRACT(0, a.len, b.str)));
}


template <class IPType, uint32_t adrsize>
void lc_trie<IPType, adrsize>::compute_branch
  (std::vector<lc_trie<IPType, adrsize>::base_t> &base,
   int prefix, int first, int n,
   int *branch, int *newprefix) const
{
  IPType low, high;
  int i, b, count;
  uint32_t pat;
  bool patfound;

  // compute the new prefix
  high = REMOVE(prefix, base[first].str);
  low = REMOVE(prefix, base[first+n-1].str);
  i = prefix;
  while(EXTRACT(i, 1, low) == EXTRACT(i, 1, high))
    ++i;
  *newprefix = i;

  // always use branching factor 2 for two elements
  if(n == 2) {
    *branch = 1;
    return;
  }

  // use a user-specified (usually large, e.g. 16) branching factor at
  // the root if given one
  if(comp_root_branching_factor > 0 && prefix == 0 && first == 0) {
    *branch = comp_root_branching_factor;
    return;
  }

  // compute the number of bits that can be used for branching.  we
  // have at least two branches.  so, we start the search at 2^b = 4
  // branches.
  b = 1;
  do {
    ++b;
    if(n < comp_fill_factor * (1<<b) || (uint32_t)(*newprefix+b) > adrsize)
      break;
    i = first;
    pat = 0;
    count = 0;
    while(pat < (uint32_t)(1<<b)) {
      patfound = false;
      while(i < first+n &&
            EXTRACT(*newprefix, b, base[i].str) == pat) {
        ++i;
        patfound = true;
      }
      if(patfound)
        ++count;
      ++pat;
    }
  } while(count >= comp_fill_factor*(1<<b));
  *branch = b-1;
}


template <class IPType, uint32_t adrsize>
void lc_trie<IPType, adrsize>::build_recursive
  (std::vector<lc_trie<IPType, adrsize>::node_t> &tree,
   std::vector<lc_trie<IPType, adrsize>::base_t> &base,
   int prefix, int first, int n, int pos, int *nextfree)
{
  int branch, newprefix;
  int k, p, adr, bits;
  uint32_t bitpat;

  if(n == 1) {
    tree[pos] = first; // branch and skip are 0
    return;
  }

  compute_branch(base, prefix, first, n, &branch, &newprefix);
  adr = *nextfree;
  tree[pos] = SETBRANCH(branch) |
              SETSKIP(newprefix - prefix) |
              SETADR(adr);
  *nextfree += 1 << branch;
  p = first;

  // build the subtrees
  for(bitpat = 0; bitpat < (uint32_t)(1<<branch); ++bitpat) {

    k = 0;
    while(p+k < first+n &&
          EXTRACT(newprefix, branch, base[p+k].str) == bitpat)
      ++k;

    if(k == 0) {
      if(p == first+n)
        build_recursive(tree, base, newprefix+branch, p-1, 1, adr+bitpat, nextfree);
      else
        build_recursive(tree, base, newprefix+branch, p, 1, adr+bitpat, nextfree);
    } else if(k == 1 && base[p].len - newprefix < branch) {
      uint32_t i;
      bits = branch + newprefix - base[p].len;
      for(i = bitpat; i < bitpat + (1<<bits); ++i)
        build_recursive(tree, base, newprefix+branch, p, 1, adr+i, nextfree);
    } else
      build_recursive(tree, base, newprefix+branch, p, k, adr+bitpat, nextfree);

    p += k;

  }
}


template <class IPType, uint32_t adrsize>
bool lc_trie<IPType, adrsize>::build
  (std::vector<lc_trie<IPType, adrsize>::input_string_t> &strings)
{
  // too many strings for our LC-trie to handle
  if(strings.size() > (1<<19))
    return false;

  // aux variables
  int nextfree = 1;

  // first, sort the prefixes
  comparator_t comp;
  std::sort(strings.begin(), strings.end(), comp);

  // next, remove duplicates
  base.push_back(strings[0]);
  for(uint32_t i = 1; i < strings.size(); ++i)
    if(comp.strcmp(strings[i-1], strings[i]) != 0)
      base.push_back(strings[i]);

  // at this point if we really wanted to condense everything down to
  // a minimal size we could eliminate all entries that are prefixed
  // by other entries, keeping only the most general strings in our
  // base vector.  however, doing so is expensive and in practice we
  // can just expect the input prefix lists to already be the most
  // general prefixes only.

  // 'base' is now the final set of inputs to our trie construction
  // algorithm; prepare initial trie and base vector.  we know that
  // the number of internal nodes in the tree can't be larger than the
  // number of strings
  trie.resize(2 * base.size() + 2000000);

  // now compile the trie
  build_recursive(trie, base, 0, 0, base.size(), 0, &nextfree);

  // we now know the exact size of the trie; get rid of unused memory
  trie.resize(nextfree);

  // clear the cached stats string just in case
  cached_stats.clear();

  return true;
}


template <class IPType, uint32_t adrsize>
bool lc_trie<IPType, adrsize>::search(const IPType &ip) const
{
  node_t node;
  int pos, branch, adr;
  IPType bitmask;

  if(trie.empty())
    return false;

  // traverse the trie
  node = trie[0];
  pos = GETSKIP(node);
  branch = GETBRANCH(node);
  adr = GETADR(node);
  while(branch != 0) {
    node = trie[adr + EXTRACT(pos, branch, ip)];
    pos += branch + GETSKIP(node);
    branch = GETBRANCH(node);
    adr = GETADR(node);
  }

  // was this a hit?
  bitmask = base[adr].str ^ ip;
  return (EXTRACT(0, base[adr].len, bitmask) == 0);
}


template <class IPType, uint32_t adrsize>
bool lc_trie<IPType, adrsize>::save(const char *filename) const
{
  std::ofstream ofs(filename, std::ios::out|std::ios::binary);
  if(ofs.fail())
    return false;

  // add a compression filter
  boost::iostreams::filtering_ostream ocfs;
  ocfs.push(boost::iostreams::gzip_compressor());
  ocfs.push(ofs);

  // serialize through the filter
  boost::archive::binary_oarchive oa(ocfs);
  oa << *this;
  return !ofs.fail();
}


template <class IPType, uint32_t adrsize>
bool lc_trie<IPType, adrsize>::load(const char *filename)
{
  std::ifstream ifs(filename, std::ios::in|std::ios::binary);
  if(ifs.fail())
    return false;

  // add a decompression filter
  boost::iostreams::filtering_istream icfs;
  icfs.push(boost::iostreams::gzip_decompressor());
  icfs.push(ifs);

  // unserialize through the filter
  boost::archive::binary_iarchive ia(icfs);
  ia >> *this;
  return !ifs.fail();
}


template <class IPType, uint32_t adrsize>
void lc_trie<IPType, adrsize>::traverse
  (const std::vector<lc_trie<IPType, adrsize>::node_t> &t,
   lc_trie<IPType, adrsize>::node_t r,
   int depth, int *totdepth, int *maxdepth) const
{
  if(GETBRANCH(r) == 0) {
    *totdepth += depth;
    if(depth > *maxdepth)
      *maxdepth = depth;
  } else
    for(int i = 0; i < 1<<GETBRANCH(r) && i < (int)t.size(); ++i)
      traverse(t, t[GETADR(r)+i], depth+1, totdepth, maxdepth);
}


template <class IPType, uint32_t adrsize>
void lc_trie<IPType, adrsize>::stats(std::string &out)
{
  if(!cached_stats.empty()) {
    out = cached_stats;
    return;
  }

  uint32_t i;
  std::ostringstream o;

  if(trie.empty()) {
    o << "Empty or not yet compiled";
    out = o.str();
    return;
  }

  uint32_t basesz = base.size()*sizeof(base_t),
    triesz = trie.size()*sizeof(node_t),
    totalsz = basesz + triesz
      + sizeof(comp_fill_factor)
      + sizeof(comp_root_branching_factor);

  o << "[N " << base.size() << "] "; // number of strings

  // memory usage
  o << "[basesz " << basesz
    << "  triesz " << triesz
    << "  totalsz " << totalsz << "] ";

  // compilation parameters
  o << "[fill " << comp_fill_factor << "  rootbranch ";
  if(comp_root_branching_factor == 0)
    o << GETBRANCH(trie[0]);
  else
    o << comp_root_branching_factor << "F";
  o << "] ";

  // counts of node types (leaves or internal nodes)
  int intnodes = 0, leaves = 0;
  for(i = 0; i < trie.size(); ++i) {
    if(GETBRANCH(trie[i]) == 0)
      ++leaves;
    else
      ++intnodes;
  }
  o << "[leaves " << leaves << "  internal " << intnodes << "] ";

  // max path length to a leaf, and average path length
  int totdepth = 0, max = 0;
  traverse(trie, trie[0], 0, &totdepth, &max);
  o << "[dmax " << max << "  davg " << (double)totdepth/leaves << "]";

  out = o.str();

  // cache the stats string since it won't change until we recompile
  // or load a new precompiled trie
  cached_stats = out;
}


//////////////////////////////////////////////////////////////////////
// implementation details: ipv4 and ipv6.  most of this stuff is small
// and should be inlined by the compiler.
//////////////////////////////////////////////////////////////////////

//// ipv4

template <>
bool strtoip<ipv4>(const char *str, ipv4 *out)
{
  uint8_t buf[4];
  if(inet_pton(AF_INET, str, buf) < 0)
    return false;
  *out = (uint32_t)buf[3] |
         ((uint32_t)buf[2] << 8) |
         ((uint32_t)buf[1] << 16) |
         ((uint32_t)buf[0] << 24);
  return true;
}

// extract n bits from str starting at position p
inline ipv4 EXTRACT(int p, int n, ipv4 str)
{
  return str << p >> (32 - n);
}

// remove the first p bits from string
inline ipv4 REMOVE(int p, ipv4 str)
{
  return str << p >> p;
}


//// ipv6


template <>
bool strtoip<ipv6>(const char *str, ipv6 *out)
{
  uint8_t buf[16];
  if(inet_pton(AF_INET6, str, buf) < 0)
    return false;
  out->hi = (uint64_t)buf[7] |
            ((uint64_t)buf[6] << 8) |
            ((uint64_t)buf[5] << 16) |
            ((uint64_t)buf[4] << 24) |
            ((uint64_t)buf[3] << 32) |
            ((uint64_t)buf[2] << 40) |
            ((uint64_t)buf[1] << 48) |
            ((uint64_t)buf[0] << 56);
  out->lo = (uint64_t)buf[15] |
            ((uint64_t)buf[14] << 8) |
            ((uint64_t)buf[13] << 16) |
            ((uint64_t)buf[12] << 24) |
            ((uint64_t)buf[11] << 32) |
            ((uint64_t)buf[10] << 40) |
            ((uint64_t)buf[9] << 48) |
            ((uint64_t)buf[8] << 56);
  return true;
}

inline bool operator<(const ipv6 &a, const ipv6 &b)
{
  return a.hi < b.hi || (a.hi == b.hi && a.lo < b.lo);
}

inline bool operator>(const ipv6 &a, const ipv6 &b)
{
  return a.hi > b.hi || (a.hi == b.hi && a.lo > b.lo);
}

inline bool operator==(const ipv6 &a, const ipv6 &b)
{
  return a.hi == b.hi && a.lo == b.lo;
}

inline bool operator==(const ipv6 &ip, uint32_t i)
{
  return ip.hi == 0 && ip.lo == i;
}

inline ipv6 operator^(const ipv6 &a, const ipv6 &b)
{
  ipv6 out = a;
  out.hi ^= b.hi;
  out.lo ^= b.lo;
  return out;
}

// just drop any bits higher than 32 from ip; this will only be used
// for smaller bit ranges than that anyway
inline uint32_t operator+(uint32_t i, const ipv6 &ip)
{
  return i + (uint32_t)(ip.lo << 32 >> 32);
}

// extract n bits from str starting at position p
inline ipv6 EXTRACT(int p, int n, ipv6 str)
{
  // FIXME can make this faster by comparing p, n, and skipping some
  // of the shifting complications if [p,p+n] doesn't overlap the
  // hi/lo boundary

  // first leftshift everything p bits
  str.hi <<= p;
  if(p > 0 && p <= 64) {
    // copy the first p bits of lo into the last p bits of hi, and
    // shift lo left p bits
    str.hi |= str.lo >> (64-p);
    str.lo <<= p;
  } else if(p > 64) {
    // shift lo to the left p-64 bits, copy it to hi, and zero lo
    str.hi = str.lo << (p-64);
    str.lo = 0;
  }

  // now rightshift everything 128-n bits, basically the reverse of
  // the above
  n = 128-n;
  str.lo >>= n;
  if(n > 0 && n < 64) {
    str.lo |= str.hi << (64-n);
    str.hi >>= n;
  } else if(n > 64) {
    str.lo = str.hi >> (n-64);
    str.hi = 0;
  } else if(n == 64) {
    str.lo = str.hi;
    str.hi = 0;
  }

  return str;
}

// remove the first p bits from string
inline ipv6 REMOVE(int p, ipv6 str)
{
  if(p > 0) {
    if(p <= 64)
      str.hi = str.hi << p >> p;
    else {
      str.hi = 0;
      p -= 64;
      str.lo = str.lo << p >> p;
    }
  }
  return str;
}


//////////////////////////////////////////////////////////////////////
// implementation details: function to compile plaintext CIDR-format
// files into LC-tries
//////////////////////////////////////////////////////////////////////

template <class IPType, uint32_t adrsize>
bool compile_lc_trie(const char *filename, lc_trie<IPType, adrsize> &trie)
{
  // first read in the file
  FILE *in = fopen(filename, "rb");
  if(!in)
    return false;

  typename lc_trie<IPType, adrsize>::input_string_t input;
  std::vector<typename lc_trie<IPType, adrsize>::input_string_t> input_vector;
  char line[256];
  char *c;
  IPType ip;
  int cidr;
  while(fscanf(in, "%256s", line) != EOF) {
    c = strchr(line, '/');
    if(c != NULL) {
      *c = 0;
      ++c;
      cidr = atoi(c);
    } else
      cidr = adrsize;

    if(!strtoip<IPType>(line, &ip))
      return false;

    input.str = ip;
    input.len = cidr;
    input_vector.push_back(input);
  }

  fclose(in);

  return trie.build(input_vector);
}


#endif // _KRB_LC_TRIE
