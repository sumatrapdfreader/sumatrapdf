// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Histogram is an object that aggregates statistics, and can summarize them in
// various forms, including ASCII graphical, HTML, and numerically (as a
// vector of numbers corresponding to each of the aggregating buckets).

// It supports calls to accumulate either time intervals (which are processed
// as integral number of milliseconds), or arbitrary integral units.

// The default layout of buckets is exponential.  For example, buckets might
// contain (sequentially) the count of values in the following intervals:
// [0,1), [1,2), [2,4), [4,8), [8,16), [16,32), [32,64), [64,infinity)
// That bucket allocation would actually result from construction of a histogram
// for values between 1 and 64, with 8 buckets, such as:
// Histogram count(L"some name", 1, 64, 8);
// Note that the underflow bucket [0,1) and the overflow bucket [64,infinity)
// are not counted by the constructor in the user supplied "bucket_count"
// argument.
// The above example has an exponential ratio of 2 (doubling the bucket width
// in each consecutive bucket.  The Histogram class automatically calculates
// the smallest ratio that it can use to construct the number of buckets
// selected in the constructor.  An another example, if you had 50 buckets,
// and millisecond time values from 1 to 10000, then the ratio between
// consecutive bucket widths will be approximately somewhere around the 50th
// root of 10000.  This approach provides very fine grain (narrow) buckets
// at the low end of the histogram scale, but allows the histogram to cover a
// gigantic range with the addition of very few buckets.

// Histograms use a pattern involving a function static variable, that is a
// pointer to a histogram.  This static is explicitly initialized on any thread
// that detects a uninitialized (NULL) pointer.  The potentially racy
// initialization is not a problem as it is always set to point to the same
// value (i.e., the FactoryGet always returns the same value).  FactoryGet
// is also completely thread safe, which results in a completely thread safe,
// and relatively fast, set of counters.  To avoid races at shutdown, the static
// pointer is NOT deleted, and we leak the histograms at process termination.

#ifndef BASE_METRICS_HISTOGRAM_H_
#define BASE_METRICS_HISTOGRAM_H_
#pragma once

#include <list>
#include <map>
#include <string>
#include <vector>

#include "base/atomicops.h"
#include "base/base_export.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/logging.h"
#include "base/time.h"

class Pickle;

namespace base {

class Lock;
//------------------------------------------------------------------------------
// Histograms are often put in areas where they are called many many times, and
// performance is critical.  As a result, they are designed to have a very low
// recurring cost of executing (adding additional samples).  Toward that end,
// the macros declare a static pointer to the histogram in question, and only
// take a "slow path" to construct (or find) the histogram on the first run
// through the macro.  We leak the histograms at shutdown time so that we don't
// have to validate using the pointers at any time during the running of the
// process.

// The following code is generally what a thread-safe static pointer
// initializaion looks like for a histogram (after a macro is expanded).  This
// sample is an expansion (with comments) of the code for
// HISTOGRAM_CUSTOM_COUNTS().

/*
  do {
    // The pointer's presence indicates the initialization is complete.
    // Initialization is idempotent, so it can safely be atomically repeated.
    static base::subtle::AtomicWord atomic_histogram_pointer = 0;

    // Acquire_Load() ensures that we acquire visibility to the pointed-to data
    // in the histogrom.
    base::Histogram* histogram_pointer(reinterpret_cast<base::Histogram*>(
        base::subtle::Acquire_Load(&atomic_histogram_pointer)));

    if (!histogram_pointer) {
      // This is the slow path, which will construct OR find the matching
      // histogram.  FactoryGet includes locks on a global histogram name map
      // and is completely thread safe.
      histogram_pointer = base::Histogram::FactoryGet(
          name, min, max, bucket_count, base::Histogram::kNoFlags);

      // Use Release_Store to ensure that the histogram data is made available
      // globally before we make the pointer visible.
      // Several threads may perform this store, but the same value will be
      // stored in all cases (for a given named/spec'ed histogram).
      // We could do this without any barrier, since FactoryGet entered and
      // exited a lock after construction, but this barrier makes things clear.
      base::subtle::Release_Store(&atomic_histogram_pointer,
          reinterpret_cast<base::subtle::AtomicWord>(histogram_pointer));
    }

    // Ensure calling contract is upheld, and the name does NOT vary.
    DCHECK(histogram_pointer->histogram_name() == constant_histogram_name);

    histogram_pointer->Add(sample);
  } while (0);
*/

// The above pattern is repeated in several macros.  The only elements that
// vary are the invocation of the Add(sample) vs AddTime(sample), and the choice
// of which FactoryGet method to use.  The different FactoryGet methods have
// various argument lists, so the function with its argument list is provided as
// a macro argument here.  The name is only used in a DCHECK, to assure that
// callers don't try to vary the name of the histogram (which would tend to be
// ignored by the one-time initialization of the histogtram_pointer).
#define STATIC_HISTOGRAM_POINTER_BLOCK(constant_histogram_name, \
                                       histogram_add_method_invocation, \
                                       histogram_factory_get_invocation) \
  do { \
    static base::subtle::AtomicWord atomic_histogram_pointer = 0; \
    base::Histogram* histogram_pointer(reinterpret_cast<base::Histogram*>( \
        base::subtle::Acquire_Load(&atomic_histogram_pointer))); \
    if (!histogram_pointer) { \
      histogram_pointer = histogram_factory_get_invocation; \
      base::subtle::Release_Store(&atomic_histogram_pointer, \
          reinterpret_cast<base::subtle::AtomicWord>(histogram_pointer)); \
    } \
    DCHECK(histogram_pointer->histogram_name() == constant_histogram_name); \
    histogram_pointer->histogram_add_method_invocation; \
  } while (0)


//------------------------------------------------------------------------------
// Provide easy general purpose histogram in a macro, just like stats counters.
// The first four macros use 50 buckets.

#define HISTOGRAM_TIMES(name, sample) HISTOGRAM_CUSTOM_TIMES( \
    name, sample, base::TimeDelta::FromMilliseconds(1), \
    base::TimeDelta::FromSeconds(10), 50)

#define HISTOGRAM_COUNTS(name, sample) HISTOGRAM_CUSTOM_COUNTS( \
    name, sample, 1, 1000000, 50)

#define HISTOGRAM_COUNTS_100(name, sample) HISTOGRAM_CUSTOM_COUNTS( \
    name, sample, 1, 100, 50)

#define HISTOGRAM_COUNTS_10000(name, sample) HISTOGRAM_CUSTOM_COUNTS( \
    name, sample, 1, 10000, 50)

#define HISTOGRAM_CUSTOM_COUNTS(name, sample, min, max, bucket_count) \
    STATIC_HISTOGRAM_POINTER_BLOCK(name, Add(sample), \
        base::Histogram::FactoryGet(name, min, max, bucket_count, \
                                    base::Histogram::kNoFlags))

#define HISTOGRAM_PERCENTAGE(name, under_one_hundred) \
    HISTOGRAM_ENUMERATION(name, under_one_hundred, 101)

// For folks that need real specific times, use this to select a precise range
// of times you want plotted, and the number of buckets you want used.
#define HISTOGRAM_CUSTOM_TIMES(name, sample, min, max, bucket_count) \
    STATIC_HISTOGRAM_POINTER_BLOCK(name, AddTime(sample), \
        base::Histogram::FactoryTimeGet(name, min, max, bucket_count, \
                                        base::Histogram::kNoFlags))

// Support histograming of an enumerated value.  The samples should always be
// strictly less than |boundary_value| -- this prevents you from running into
// problems down the line if you add additional buckets to the histogram.  Note
// also that, despite explicitly setting the minimum bucket value to |1| below,
// it is fine for enumerated histograms to be 0-indexed -- this is because
// enumerated histograms should never have underflow.
#define HISTOGRAM_ENUMERATION(name, sample, boundary_value) \
    STATIC_HISTOGRAM_POINTER_BLOCK(name, Add(sample), \
        base::LinearHistogram::FactoryGet(name, 1, boundary_value, \
            boundary_value + 1, base::Histogram::kNoFlags))

// Support histograming of an enumerated value. Samples should be one of the
// std::vector<int> list provided via |custom_ranges|. You can use the helper
// function |base::CustomHistogram::ArrayToCustomRanges(samples, num_samples)|
// to transform a C-style array of valid sample values to a std::vector<int>.
#define HISTOGRAM_CUSTOM_ENUMERATION(name, sample, custom_ranges) \
    STATIC_HISTOGRAM_POINTER_BLOCK(name, Add(sample), \
        base::CustomHistogram::FactoryGet(name, custom_ranges, \
                                          base::Histogram::kNoFlags))

//------------------------------------------------------------------------------
// Define Debug vs non-debug flavors of macros.
#ifndef NDEBUG

#define DHISTOGRAM_TIMES(name, sample) HISTOGRAM_TIMES(name, sample)
#define DHISTOGRAM_COUNTS(name, sample) HISTOGRAM_COUNTS(name, sample)
#define DHISTOGRAM_PERCENTAGE(name, under_one_hundred) HISTOGRAM_PERCENTAGE(\
    name, under_one_hundred)
#define DHISTOGRAM_CUSTOM_TIMES(name, sample, min, max, bucket_count) \
    HISTOGRAM_CUSTOM_TIMES(name, sample, min, max, bucket_count)
#define DHISTOGRAM_CLIPPED_TIMES(name, sample, min, max, bucket_count) \
    HISTOGRAM_CLIPPED_TIMES(name, sample, min, max, bucket_count)
#define DHISTOGRAM_CUSTOM_COUNTS(name, sample, min, max, bucket_count) \
    HISTOGRAM_CUSTOM_COUNTS(name, sample, min, max, bucket_count)
#define DHISTOGRAM_ENUMERATION(name, sample, boundary_value) \
    HISTOGRAM_ENUMERATION(name, sample, boundary_value)
#define DHISTOGRAM_CUSTOM_ENUMERATION(name, sample, custom_ranges) \
    HISTOGRAM_CUSTOM_ENUMERATION(name, sample, custom_ranges)

#else  // NDEBUG
// Keep a mention of passed variables to avoid unused variable warnings in
// release build if these variables are only used in macros.
#define DISCARD_2_ARGUMENTS(a, b) \
  while (0) { \
    static_cast<void>(a); \
    static_cast<void>(b); \
 }
#define DISCARD_3_ARGUMENTS(a, b, c) \
  while (0) { \
    static_cast<void>(a); \
    static_cast<void>(b); \
    static_cast<void>(c); \
 }
#define DISCARD_5_ARGUMENTS(a, b, c, d ,e) \
  while (0) { \
    static_cast<void>(a); \
    static_cast<void>(b); \
    static_cast<void>(c); \
    static_cast<void>(d); \
    static_cast<void>(e); \
 }
#define DHISTOGRAM_TIMES(name, sample) \
    DISCARD_2_ARGUMENTS(name, sample)

#define DHISTOGRAM_COUNTS(name, sample) \
    DISCARD_2_ARGUMENTS(name, sample)

#define DHISTOGRAM_PERCENTAGE(name, under_one_hundred) \
    DISCARD_2_ARGUMENTS(name, under_one_hundred)

#define DHISTOGRAM_CUSTOM_TIMES(name, sample, min, max, bucket_count) \
    DISCARD_5_ARGUMENTS(name, sample, min, max, bucket_count)

#define DHISTOGRAM_CLIPPED_TIMES(name, sample, min, max, bucket_count) \
    DISCARD_5_ARGUMENTS(name, sample, min, max, bucket_count)

#define DHISTOGRAM_CUSTOM_COUNTS(name, sample, min, max, bucket_count) \
    DISCARD_5_ARGUMENTS(name, sample, min, max, bucket_count)

#define DHISTOGRAM_ENUMERATION(name, sample, boundary_value) \
    DISCARD_3_ARGUMENTS(name, sample, boundary_value)

#define DHISTOGRAM_CUSTOM_ENUMERATION(name, sample, custom_ranges) \
    DISCARD_3_ARGUMENTS(name, sample, custom_ranges)

#endif  // NDEBUG

//------------------------------------------------------------------------------
// The following macros provide typical usage scenarios for callers that wish
// to record histogram data, and have the data submitted/uploaded via UMA.
// Not all systems support such UMA, but if they do, the following macros
// should work with the service.

#define UMA_HISTOGRAM_TIMES(name, sample) UMA_HISTOGRAM_CUSTOM_TIMES( \
    name, sample, base::TimeDelta::FromMilliseconds(1), \
    base::TimeDelta::FromSeconds(10), 50)

#define UMA_HISTOGRAM_MEDIUM_TIMES(name, sample) UMA_HISTOGRAM_CUSTOM_TIMES( \
    name, sample, base::TimeDelta::FromMilliseconds(10), \
    base::TimeDelta::FromMinutes(3), 50)

// Use this macro when times can routinely be much longer than 10 seconds.
#define UMA_HISTOGRAM_LONG_TIMES(name, sample) UMA_HISTOGRAM_CUSTOM_TIMES( \
    name, sample, base::TimeDelta::FromMilliseconds(1), \
    base::TimeDelta::FromHours(1), 50)

#define UMA_HISTOGRAM_CUSTOM_TIMES(name, sample, min, max, bucket_count) \
    STATIC_HISTOGRAM_POINTER_BLOCK(name, AddTime(sample), \
        base::Histogram::FactoryTimeGet(name, min, max, bucket_count, \
            base::Histogram::kUmaTargetedHistogramFlag))

#define UMA_HISTOGRAM_COUNTS(name, sample) UMA_HISTOGRAM_CUSTOM_COUNTS( \
    name, sample, 1, 1000000, 50)

#define UMA_HISTOGRAM_COUNTS_100(name, sample) UMA_HISTOGRAM_CUSTOM_COUNTS( \
    name, sample, 1, 100, 50)

#define UMA_HISTOGRAM_COUNTS_10000(name, sample) UMA_HISTOGRAM_CUSTOM_COUNTS( \
    name, sample, 1, 10000, 50)

#define UMA_HISTOGRAM_CUSTOM_COUNTS(name, sample, min, max, bucket_count) \
    STATIC_HISTOGRAM_POINTER_BLOCK(name, Add(sample), \
        base::Histogram::FactoryGet(name, min, max, bucket_count, \
            base::Histogram::kUmaTargetedHistogramFlag))

#define UMA_HISTOGRAM_MEMORY_KB(name, sample) UMA_HISTOGRAM_CUSTOM_COUNTS( \
    name, sample, 1000, 500000, 50)

#define UMA_HISTOGRAM_MEMORY_MB(name, sample) UMA_HISTOGRAM_CUSTOM_COUNTS( \
    name, sample, 1, 1000, 50)

#define UMA_HISTOGRAM_PERCENTAGE(name, under_one_hundred) \
    UMA_HISTOGRAM_ENUMERATION(name, under_one_hundred, 101)

#define UMA_HISTOGRAM_BOOLEAN(name, sample) \
    STATIC_HISTOGRAM_POINTER_BLOCK(name, AddBoolean(sample), \
        base::BooleanHistogram::FactoryGet(name, \
            base::Histogram::kUmaTargetedHistogramFlag))

// The samples should always be strictly less than |boundary_value|.  For more
// details, see the comment for the |HISTOGRAM_ENUMERATION| macro, above.
#define UMA_HISTOGRAM_ENUMERATION(name, sample, boundary_value) \
    STATIC_HISTOGRAM_POINTER_BLOCK(name, Add(sample), \
        base::LinearHistogram::FactoryGet(name, 1, boundary_value, \
            boundary_value + 1, base::Histogram::kUmaTargetedHistogramFlag))

#define UMA_HISTOGRAM_CUSTOM_ENUMERATION(name, sample, custom_ranges) \
    STATIC_HISTOGRAM_POINTER_BLOCK(name, Add(sample), \
        base::CustomHistogram::FactoryGet(name, custom_ranges, \
            base::Histogram::kUmaTargetedHistogramFlag))

//------------------------------------------------------------------------------

class BooleanHistogram;
class CachedRanges;
class CustomHistogram;
class Histogram;
class LinearHistogram;

class BASE_EXPORT Histogram {
 public:
  typedef int Sample;  // Used for samples (and ranges of samples).
  typedef int Count;  // Used to count samples in a bucket.
  static const Sample kSampleType_MAX = INT_MAX;
  // Initialize maximum number of buckets in histograms as 16,384.
  static const size_t kBucketCount_MAX;

  typedef std::vector<Count> Counts;

  // These enums are used to facilitate deserialization of renderer histograms
  // into the browser.
  enum ClassType {
    HISTOGRAM,
    LINEAR_HISTOGRAM,
    BOOLEAN_HISTOGRAM,
    CUSTOM_HISTOGRAM,
    NOT_VALID_IN_RENDERER
  };

  enum BucketLayout {
    EXPONENTIAL,
    LINEAR,
    CUSTOM
  };

  enum Flags {
    kNoFlags = 0,
    kUmaTargetedHistogramFlag = 0x1,  // Histogram should be UMA uploaded.

    // Indicate that the histogram was pickled to be sent across an IPC Channel.
    // If we observe this flag on a histogram being aggregated into after IPC,
    // then we are running in a single process mode, and the aggregation should
    // not take place (as we would be aggregating back into the source
    // histogram!).
    kIPCSerializationSourceFlag = 0x10,

    kHexRangePrintingFlag = 0x8000,  // Fancy bucket-naming supported.
  };

  enum Inconsistencies {
    NO_INCONSISTENCIES = 0x0,
    RANGE_CHECKSUM_ERROR = 0x1,
    BUCKET_ORDER_ERROR = 0x2,
    COUNT_HIGH_ERROR = 0x4,
    COUNT_LOW_ERROR = 0x8,

    NEVER_EXCEEDED_VALUE = 0x10
  };

  struct DescriptionPair {
    Sample sample;
    const char* description;  // Null means end of a list of pairs.
  };

  //----------------------------------------------------------------------------
  // Statistic values, developed over the life of the histogram.

  class BASE_EXPORT SampleSet {
   public:
    explicit SampleSet();
    ~SampleSet();

    // Adjust size of counts_ for use with given histogram.
    void Resize(const Histogram& histogram);
    void CheckSize(const Histogram& histogram) const;

    // Accessor for histogram to make routine additions.
    void Accumulate(Sample value, Count count, size_t index);

    // Accessor methods.
    Count counts(size_t i) const { return counts_[i]; }
    Count TotalCount() const;
    int64 sum() const { return sum_; }
    int64 redundant_count() const { return redundant_count_; }

    // Arithmetic manipulation of corresponding elements of the set.
    void Add(const SampleSet& other);
    void Subtract(const SampleSet& other);

    bool Serialize(Pickle* pickle) const;
    bool Deserialize(void** iter, const Pickle& pickle);

   protected:
    // Actual histogram data is stored in buckets, showing the count of values
    // that fit into each bucket.
    Counts counts_;

    // Save simple stats locally.  Note that this MIGHT get done in base class
    // without shared memory at some point.
    int64 sum_;         // sum of samples.

   private:
    // Allow tests to corrupt our innards for testing purposes.
    FRIEND_TEST_ALL_PREFIXES(HistogramTest, CorruptSampleCounts);

    // To help identify memory corruption, we reduntantly save the number of
    // samples we've accumulated into all of our buckets.  We can compare this
    // count to the sum of the counts in all buckets, and detect problems.  Note
    // that due to races in histogram accumulation (if a histogram is indeed
    // updated on several threads simultaneously), the tallies might mismatch,
    // and also the snapshotting code may asynchronously get a mismatch (though
    // generally either race based mismatch cause is VERY rare).
    int64 redundant_count_;
  };

  //----------------------------------------------------------------------------
  // For a valid histogram, input should follow these restrictions:
  // minimum > 0 (if a minimum below 1 is specified, it will implicitly be
  //              normalized up to 1)
  // maximum > minimum
  // buckets > 2 [minimum buckets needed: underflow, overflow and the range]
  // Additionally,
  // buckets <= (maximum - minimum + 2) - this is to ensure that we don't have
  // more buckets than the range of numbers; having more buckets than 1 per
  // value in the range would be nonsensical.
  static Histogram* FactoryGet(const std::string& name,
                               Sample minimum,
                               Sample maximum,
                               size_t bucket_count,
                               Flags flags);
  static Histogram* FactoryTimeGet(const std::string& name,
                                   base::TimeDelta minimum,
                                   base::TimeDelta maximum,
                                   size_t bucket_count,
                                   Flags flags);
  // Time call for use with DHISTOGRAM*.
  // Returns TimeTicks::Now() in debug and TimeTicks() in release build.
  static TimeTicks DebugNow();

  void Add(int value);

  // This method is an interface, used only by BooleanHistogram.
  virtual void AddBoolean(bool value);

  // Accept a TimeDelta to increment.
  void AddTime(TimeDelta time) {
    Add(static_cast<int>(time.InMilliseconds()));
  }

  void AddSampleSet(const SampleSet& sample);

  // This method is an interface, used only by LinearHistogram.
  virtual void SetRangeDescriptions(const DescriptionPair descriptions[]);

  // The following methods provide graphical histogram displays.
  void WriteHTMLGraph(std::string* output) const;
  void WriteAscii(bool graph_it, const std::string& newline,
                  std::string* output) const;

  // Support generic flagging of Histograms.
  // 0x1 Currently used to mark this histogram to be recorded by UMA..
  // 0x8000 means print ranges in hex.
  void SetFlags(Flags flags) { flags_ = static_cast<Flags> (flags_ | flags); }
  void ClearFlags(Flags flags) { flags_ = static_cast<Flags>(flags_ & ~flags); }
  int flags() const { return flags_; }

  // Convenience methods for serializing/deserializing the histograms.
  // Histograms from Renderer process are serialized and sent to the browser.
  // Browser process reconstructs the histogram from the pickled version
  // accumulates the browser-side shadow copy of histograms (that mirror
  // histograms created in the renderer).

  // Serialize the given snapshot of a Histogram into a String. Uses
  // Pickle class to flatten the object.
  static std::string SerializeHistogramInfo(const Histogram& histogram,
                                            const SampleSet& snapshot);

  // The following method accepts a list of pickled histograms and
  // builds a histogram and updates shadow copy of histogram data in the
  // browser process.
  static bool DeserializeHistogramInfo(const std::string& histogram_info);

  // Check to see if bucket ranges, counts and tallies in the snapshot are
  // consistent with the bucket ranges and checksums in our histogram.  This can
  // produce a false-alarm if a race occurred in the reading of the data during
  // a SnapShot process, but should otherwise be false at all times (unless we
  // have memory over-writes, or DRAM failures).
  virtual Inconsistencies FindCorruption(const SampleSet& snapshot) const;

  //----------------------------------------------------------------------------
  // Accessors for factory constuction, serialization and testing.
  //----------------------------------------------------------------------------
  virtual ClassType histogram_type() const;
  const std::string& histogram_name() const { return histogram_name_; }
  Sample declared_min() const { return declared_min_; }
  Sample declared_max() const { return declared_max_; }
  virtual Sample ranges(size_t i) const;
  uint32 range_checksum() const { return range_checksum_; }
  virtual size_t bucket_count() const;
  CachedRanges* cached_ranges() const { return cached_ranges_; }
  void set_cached_ranges(CachedRanges* cached_ranges) {
    cached_ranges_ = cached_ranges;
  }
  // Snapshot the current complete set of sample data.
  // Override with atomic/locked snapshot if needed.
  virtual void SnapshotSample(SampleSet* sample) const;

  virtual bool HasConstructorArguments(Sample minimum, Sample maximum,
                                       size_t bucket_count);

  virtual bool HasConstructorTimeDeltaArguments(TimeDelta minimum,
                                                TimeDelta maximum,
                                                size_t bucket_count);
  // Return true iff the range_checksum_ matches current |ranges_| vector in
  // |cached_ranges_|.
  bool HasValidRangeChecksum() const;

 protected:
  Histogram(const std::string& name, Sample minimum,
            Sample maximum, size_t bucket_count);
  Histogram(const std::string& name, TimeDelta minimum,
            TimeDelta maximum, size_t bucket_count);

  virtual ~Histogram();

  // Serialize the histogram's ranges to |*pickle|, returning true on success.
  // Most subclasses can leave this no-op implementation, but some will want to
  // override it, especially if the ranges cannot be re-derived from other
  // serialized parameters.
  virtual bool SerializeRanges(Pickle* pickle) const;

  // Initialize ranges_ mapping in cached_ranges_.
  void InitializeBucketRange();

  // Method to override to skip the display of the i'th bucket if it's empty.
  virtual bool PrintEmptyBucket(size_t index) const;

  //----------------------------------------------------------------------------
  // Methods to override to create histogram with different bucket widths.
  //----------------------------------------------------------------------------
  // Find bucket to increment for sample value.
  virtual size_t BucketIndex(Sample value) const;
  // Get normalized size, relative to the ranges(i).
  virtual double GetBucketSize(Count current, size_t i) const;

  // Recalculate range_checksum_.
  void ResetRangeChecksum();

  // Return a string description of what goes in a given bucket.
  // Most commonly this is the numeric value, but in derived classes it may
  // be a name (or string description) given to the bucket.
  virtual const std::string GetAsciiBucketRange(size_t it) const;

  //----------------------------------------------------------------------------
  // Methods to override to create thread safe histogram.
  //----------------------------------------------------------------------------
  // Update all our internal data, including histogram
  virtual void Accumulate(Sample value, Count count, size_t index);

  //----------------------------------------------------------------------------
  // Accessors for derived classes.
  //----------------------------------------------------------------------------
  void SetBucketRange(size_t i, Sample value);

  // Validate that ranges_ in cached_ranges_ was created sensibly (top and
  // bottom range values relate properly to the declared_min_ and
  // declared_max_).
  bool ValidateBucketRanges() const;

  virtual uint32 CalculateRangeChecksum() const;

 private:
  // Allow tests to corrupt our innards for testing purposes.
  FRIEND_TEST_ALL_PREFIXES(HistogramTest, CorruptBucketBounds);
  FRIEND_TEST_ALL_PREFIXES(HistogramTest, CorruptSampleCounts);
  FRIEND_TEST_ALL_PREFIXES(HistogramTest, Crc32SampleHash);
  FRIEND_TEST_ALL_PREFIXES(HistogramTest, Crc32TableTest);

  friend class StatisticsRecorder;  // To allow it to delete duplicates.

  // Post constructor initialization.
  void Initialize();

  // Checksum function for accumulating range values into a checksum.
  static uint32 Crc32(uint32 sum, Sample range);

  //----------------------------------------------------------------------------
  // Helpers for emitting Ascii graphic.  Each method appends data to output.

  // Find out how large the (graphically) the largest bucket will appear to be.
  double GetPeakBucketSize(const SampleSet& snapshot) const;

  // Write a common header message describing this histogram.
  void WriteAsciiHeader(const SampleSet& snapshot,
                        Count sample_count, std::string* output) const;

  // Write information about previous, current, and next buckets.
  // Information such as cumulative percentage, etc.
  void WriteAsciiBucketContext(const int64 past, const Count current,
                               const int64 remaining, const size_t i,
                               std::string* output) const;

  // Write textual description of the bucket contents (relative to histogram).
  // Output is the count in the buckets, as well as the percentage.
  void WriteAsciiBucketValue(Count current, double scaled_sum,
                             std::string* output) const;

  // Produce actual graph (set of blank vs non blank char's) for a bucket.
  void WriteAsciiBucketGraph(double current_size, double max_size,
                             std::string* output) const;

  //----------------------------------------------------------------------------
  // Table for generating Crc32 values.
  static const uint32 kCrcTable[256];
  //----------------------------------------------------------------------------
  // Invariant values set at/near construction time

  // ASCII version of original name given to the constructor.  All identically
  // named instances will be coalesced cross-project.
  const std::string histogram_name_;
  Sample declared_min_;  // Less than this goes into counts_[0]
  Sample declared_max_;  // Over this goes into counts_[bucket_count_ - 1].
  size_t bucket_count_;  // Dimension of counts_[].

  // Flag the histogram for recording by UMA via metric_services.h.
  Flags flags_;

  // For each index, show the least value that can be stored in the
  // corresponding bucket. We also append one extra element in this array,
  // containing kSampleType_MAX, to make calculations easy.
  // The dimension of ranges_ in cached_ranges_ is bucket_count + 1.
  CachedRanges* cached_ranges_;

  // For redundancy, we store a checksum of all the sample ranges when ranges
  // are generated.  If ever there is ever a difference, then the histogram must
  // have been corrupted.
  uint32 range_checksum_;

  // Finally, provide the state that changes with the addition of each new
  // sample.
  SampleSet sample_;

  DISALLOW_COPY_AND_ASSIGN(Histogram);
};

//------------------------------------------------------------------------------

// LinearHistogram is a more traditional histogram, with evenly spaced
// buckets.
class BASE_EXPORT LinearHistogram : public Histogram {
 public:
  virtual ~LinearHistogram();

  /* minimum should start from 1. 0 is as minimum is invalid. 0 is an implicit
     default underflow bucket. */
  static Histogram* FactoryGet(const std::string& name,
                               Sample minimum,
                               Sample maximum,
                               size_t bucket_count,
                               Flags flags);
  static Histogram* FactoryTimeGet(const std::string& name,
                                   TimeDelta minimum,
                                   TimeDelta maximum,
                                   size_t bucket_count,
                                   Flags flags);

  // Overridden from Histogram:
  virtual ClassType histogram_type() const OVERRIDE;

  // Store a list of number/text values for use in rendering the histogram.
  // The last element in the array has a null in its "description" slot.
  virtual void SetRangeDescriptions(
      const DescriptionPair descriptions[]) OVERRIDE;

 protected:
  LinearHistogram(const std::string& name, Sample minimum,
                  Sample maximum, size_t bucket_count);

  LinearHistogram(const std::string& name, TimeDelta minimum,
                  TimeDelta maximum, size_t bucket_count);

  // Initialize ranges_ mapping in cached_ranges_.
  void InitializeBucketRange();
  virtual double GetBucketSize(Count current, size_t i) const OVERRIDE;

  // If we have a description for a bucket, then return that.  Otherwise
  // let parent class provide a (numeric) description.
  virtual const std::string GetAsciiBucketRange(size_t i) const OVERRIDE;

  // Skip printing of name for numeric range if we have a name (and if this is
  // an empty bucket).
  virtual bool PrintEmptyBucket(size_t index) const OVERRIDE;

 private:
  // For some ranges, we store a printable description of a bucket range.
  // If there is no desciption, then GetAsciiBucketRange() uses parent class
  // to provide a description.
  typedef std::map<Sample, std::string> BucketDescriptionMap;
  BucketDescriptionMap bucket_description_;

  DISALLOW_COPY_AND_ASSIGN(LinearHistogram);
};

//------------------------------------------------------------------------------

// BooleanHistogram is a histogram for booleans.
class BASE_EXPORT BooleanHistogram : public LinearHistogram {
 public:
  static Histogram* FactoryGet(const std::string& name, Flags flags);

  virtual ClassType histogram_type() const OVERRIDE;

  virtual void AddBoolean(bool value) OVERRIDE;

 private:
  explicit BooleanHistogram(const std::string& name);

  DISALLOW_COPY_AND_ASSIGN(BooleanHistogram);
};

//------------------------------------------------------------------------------

// CustomHistogram is a histogram for a set of custom integers.
class BASE_EXPORT CustomHistogram : public Histogram {
 public:

  static Histogram* FactoryGet(const std::string& name,
                               const std::vector<Sample>& custom_ranges,
                               Flags flags);

  // Overridden from Histogram:
  virtual ClassType histogram_type() const OVERRIDE;

  // Helper method for transforming an array of valid enumeration values
  // to the std::vector<int> expected by HISTOGRAM_CUSTOM_ENUMERATION.
  // This function ensures that a guard bucket exists right after any
  // valid sample value (unless the next higher sample is also a valid value),
  // so that invalid samples never fall into the same bucket as valid samples.
  static std::vector<Sample> ArrayToCustomRanges(const Sample* values,
                                                 size_t num_values);

  // Helper for deserializing CustomHistograms.  |*ranges| should already be
  // correctly sized before this call.  Return true on success.
  static bool DeserializeRanges(void** iter, const Pickle& pickle,
                                std::vector<Histogram::Sample>* ranges);


 protected:
  CustomHistogram(const std::string& name,
                  const std::vector<Sample>& custom_ranges);

  virtual bool SerializeRanges(Pickle* pickle) const OVERRIDE;

  // Initialize ranges_ mapping in cached_ranges_.
  void InitializedCustomBucketRange(const std::vector<Sample>& custom_ranges);
  virtual double GetBucketSize(Count current, size_t i) const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(CustomHistogram);
};

//------------------------------------------------------------------------------
// StatisticsRecorder handles all histograms in the system.  It provides a
// general place for histograms to register, and supports a global API for
// accessing (i.e., dumping, or graphing) the data in all the histograms.

class BASE_EXPORT StatisticsRecorder {
 public:
  typedef std::vector<Histogram*> Histograms;

  StatisticsRecorder();

  ~StatisticsRecorder();

  // Find out if histograms can now be registered into our list.
  static bool IsActive();

  // Register, or add a new histogram to the collection of statistics. If an
  // identically named histogram is already registered, then the argument
  // |histogram| will deleted.  The returned value is always the registered
  // histogram (either the argument, or the pre-existing registered histogram).
  static Histogram* RegisterOrDeleteDuplicate(Histogram* histogram);

  // Register, or add a new cached_ranges_ of |histogram|. If an identical
  // cached_ranges_ is already registered, then the cached_ranges_ of
  // |histogram| is deleted and the |histogram|'s cached_ranges_ is reset to the
  // registered cached_ranges_.  The cached_ranges_ of |histogram| is always the
  // registered CachedRanges (either the argument's cached_ranges_, or the
  // pre-existing registered cached_ranges_).
  static void RegisterOrDeleteDuplicateRanges(Histogram* histogram);

  // Method for collecting stats about histograms created in browser and
  // renderer processes. |suffix| is appended to histogram names. |suffix| could
  // be either browser or renderer.
  static void CollectHistogramStats(const std::string& suffix);

  // Methods for printing histograms.  Only histograms which have query as
  // a substring are written to output (an empty string will process all
  // registered histograms).
  static void WriteHTMLGraph(const std::string& query, std::string* output);
  static void WriteGraph(const std::string& query, std::string* output);

  // Method for extracting histograms which were marked for use by UMA.
  static void GetHistograms(Histograms* output);

  // Find a histogram by name. It matches the exact name. This method is thread
  // safe.  If a matching histogram is not found, then the |histogram| is
  // not changed.
  static bool FindHistogram(const std::string& query, Histogram** histogram);

  static bool dump_on_exit() { return dump_on_exit_; }

  static void set_dump_on_exit(bool enable) { dump_on_exit_ = enable; }

  // GetSnapshot copies some of the pointers to registered histograms into the
  // caller supplied vector (Histograms).  Only histograms with names matching
  // query are returned. The query must be a substring of histogram name for its
  // pointer to be copied.
  static void GetSnapshot(const std::string& query, Histograms* snapshot);


 private:
  // We keep all registered histograms in a map, from name to histogram.
  typedef std::map<std::string, Histogram*> HistogramMap;

  // We keep all |cached_ranges_| in a map, from checksum to a list of
  // |cached_ranges_|.  Checksum is calculated from the |ranges_| in
  // |cached_ranges_|.
  typedef std::map<uint32, std::list<CachedRanges*>*> RangesMap;

  static HistogramMap* histograms_;

  static RangesMap* ranges_;

  // lock protects access to the above map.
  static base::Lock* lock_;

  // Dump all known histograms to log.
  static bool dump_on_exit_;

  DISALLOW_COPY_AND_ASSIGN(StatisticsRecorder);
};

//------------------------------------------------------------------------------

// CachedRanges stores the Ranges vector. Histograms that have same Ranges
// vector will use the same CachedRanges object.
class BASE_EXPORT CachedRanges {
 public:
  typedef std::vector<Histogram::Sample> Ranges;

  CachedRanges(size_t bucket_count, int initial_value);
  ~CachedRanges();

  //----------------------------------------------------------------------------
  // Accessors methods for ranges_ and range_checksum_.
  //----------------------------------------------------------------------------
  size_t size() const { return ranges_.size(); }
  Histogram::Sample ranges(size_t i) const { return ranges_[i]; }
  void SetBucketRange(size_t i, Histogram::Sample value);
  uint32 range_checksum(uint32 checksum) const { return range_checksum_; }
  void SetRangeChecksum(uint32 checksum) { range_checksum_ = checksum; }

  // Return true iff |other| object has same ranges_ as |this| object's ranges_.
  bool Equals(CachedRanges* other) const;

 private:
  // Allow tests to corrupt our innards for testing purposes.
  FRIEND_TEST_ALL_PREFIXES(HistogramTest, CorruptBucketBounds);

  // A monotonically increasing list of values which determine which bucket to
  // put a sample into.  For each index, show the smallest sample that can be
  // added to the corresponding bucket.
  Ranges ranges_;

  // Checksum for the conntents of ranges_.  Used to detect random over-writes
  // of our data, and to quickly see if some other CachedRanges instance is
  // possibly Equal() to this instance.
  uint32 range_checksum_;

  DISALLOW_COPY_AND_ASSIGN(CachedRanges);
};

}  // namespace base

#endif  // BASE_METRICS_HISTOGRAM_H_
