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
  Working set size estimation.  This is basically a small set of Bloom
  filters where every time an new object is added that doesn't exist
  in any of the Bloom filters, its size is added to the working set
  size for the "current" filter.  We estimate the WSS at some
  "interval resolution" --- every interval gets its own Bloom filter,
  and we keep enough intervals to cover the desired working set
  period.
 */

#ifndef _KRB_WSS_ESTIMATOR_HPP
#define _KRB_WSS_ESTIMATOR_HPP

#include <krb/bloom_filter.hpp>
#include <list>

template <class size_type = uint64_t>
class wss_estimator
{

public:

  // parameters:
  //
  //  num_intervals: the number of intervals to keep.  there is one
  //  bloom filter per interval.  the total working set period is
  //  num_intervals * interval length.
  //
  //  elements_per_interval: upper limit on the expected number of
  //  items in the working set for each interval.  when in doubt, be
  //  conservative with this, as if you pick a number too small the
  //  false positive rate will climb beyond the requested amount.
  //
  //  false_pos_rate: the desired maximum rate of false positives
  //  (should be in [0..1]).
  //
  //  adaptive_filter_size_buffer (optional): if nonzero, we'll
  //  adaptively adjust the size of the bloom filters we use for each
  //  interval to try to use no more (or no less) memory than we
  //  really need to to represent the working set with the desired
  //  accuracy.  this parameter is the percentage of the predicted
  //  interval set size to tack on as extra "buffer" in case the next
  //  interval's working set is larger than predicted.
  wss_estimator
    (uint32_t num_intervals,
     uint32_t elements_per_interval,
     double false_pos_rate,
     double adaptive_filter_size_buffer = 0.0);

  // call this to check for the existence of an item in the working
  // set, and add it if it's not already there
  void add(const void *key, uint32_t sz, uint32_t bytes);

  // call this at the end of every interval (e.g., if you're
  // estimating the 5-min working set using 5 1-min intervals, call
  // this once a minute)
  void end_interval();

  // call this to get the current working set size estimate, computed
  // by summing the sizes of all the intervals.  this is always an
  // underestimate, for two reasons:

  //  1. the Bloom filters admit false positives, so fp_rate % of the
  //  objects you add will not be counted toward the working set.

  //  2. the most recent interval is always "incomplete"; depending on
  //  how recently the last interval ended, you may lose a lot of
  //  accuracy.

  // we can try to make an "educated guess" to overcome these
  // obstacles based on some statistics about other intervals and our
  // knowledge of the fp_rate.  if you want the "educated guess"
  // estimate, call best_guess() with the percentage of the current
  // interval that's passed (must be in [0,1]).
  size_type size() const;
  size_type best_guess(double interval_percent) const;

  // returns the total number of bloom filter buckets in use by this
  // estimator; the total memory usage of the estimator is
  // ~buckets()/8.
  uint32_t buckets() const;

protected:

  typedef std::pair<bloom_filter, size_type> interval_t;
  typedef std::list<interval_t> interval_list;
  typedef typename interval_list::const_iterator interval_iterator;

  interval_list filters;
  uint32_t N, E, cur_set_size;
  double fp_rate, adaptive_buffer_perc;
  size_type last_discarded_size;

};


//////////////////////////////////////////////////////////////////////
// implementation details
//////////////////////////////////////////////////////////////////////

template <class size_type>
wss_estimator<size_type>::wss_estimator
  (uint32_t num_intervals,
   uint32_t elements_per_interval,
   double false_pos_rate,
   double adaptive_filter_size_buffer)
    : N(num_intervals), E(elements_per_interval),
      cur_set_size(0), fp_rate(false_pos_rate),
      adaptive_buffer_perc(adaptive_filter_size_buffer),
      last_discarded_size(0)
{
  filters.push_front(interval_t(bloom_filter(E, fp_rate), 0));
}

template <class size_type>
void wss_estimator<size_type>::add
  (const void *key, uint32_t sz, uint32_t bytes)
{
  // first check if this key is already in the working set by querying
  // all our interval filters
  for(interval_iterator i = filters.begin(); i != filters.end(); ++i)
    if(i->first.query(key, sz))
      return; // already in the working set

  // nope, add it to the current interval's filter
  filters.front().first.add(key, sz);
  filters.front().second += bytes;
  ++cur_set_size;
}

template <class size_type>
void wss_estimator<size_type>::end_interval()
{
  uint32_t next_E = E;

  if(adaptive_buffer_perc > 0) {
    // just use the current interval's set size as our prediction for
    // the next interval, and add some extra space in case the set
    // size grows; add in an extra 20 just in case we see an empty
    // interval.
    next_E = cur_set_size +
      (uint32_t)(cur_set_size * adaptive_buffer_perc) +
      20;
  }

  filters.push_front(interval_t(bloom_filter(next_E, fp_rate), 0));

  if(filters.size() > N) {
    last_discarded_size = filters.back().second;
    filters.pop_back();
  }

  cur_set_size = 0;
}

template <class size_type>
size_type wss_estimator<size_type>::size() const
{
  size_type S = 0;
  for(interval_iterator i = filters.begin(); i != filters.end(); ++i)
    S += i->second;
  return S;
}

// our basic strategy here is to keep the size of the last discarded
// interval (T-N-1) around; we compute the basic size as SIZE =
// sum(S[T-N..T-1]) + x*S[T] + (1-x)*S[T-N-1].  this is easy and
// reasonable.  then, we add to this fp_rate*SIZE.
//
// we have to do something completely different if we haven't
// completed enough intervals to cover a full working set period.
// then we just project our current data forward to to estimate the
// missing data before adding fp_rate*SIZE.
template <class size_type>
size_type wss_estimator<size_type>::best_guess
  (double interval_percent) const
{
  size_type S = 0;
  interval_iterator i = filters.begin();

  if(filters.size() == N) {

    // we've got data for a full working set period

    for(++i; i != filters.end(); ++i)
      S += i->second;

    S += interval_percent * S.front().second +
      (1 - interval_percent) * last_discarded_size;

  } else {

    // crap, not enough data for a full working set period.  figure
    // out the fraction of a working set period we _do_ know about,
    // then estimate based on our data so far.

    for(; i != filters.end(); ++i)
      S += i->second;

    double frac = double(filters.size()-1 + interval_percent) / N;
    S /= frac;

  }

  // account for Bloom filter false positives as best we can
  S += fp_rate * S;

  return S;

}

template <class size_type>
uint32_t wss_estimator<size_type>::buckets() const
{
  uint32_t B = 0;
  for(interval_iterator i = filters.begin(); i != filters.end(); ++i)
    B += i->first.buckets();
  return B;
}


#endif // _KRB_WSS_ESTIMATOR_HPP
