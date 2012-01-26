// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TRACKED_OBJECTS_H_
#define BASE_TRACKED_OBJECTS_H_
#pragma once

#include <map>
#include <set>
#include <stack>
#include <string>
#include <utility>
#include <vector>

#include "base/base_export.h"
#include "base/gtest_prod_util.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/profiler/tracked_time.h"
#include "base/time.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_local_storage.h"
#include "base/tracking_info.h"
#include "base/values.h"

// TrackedObjects provides a database of stats about objects (generally Tasks)
// that are tracked.  Tracking means their birth, death, duration, birth thread,
// death thread, and birth place are recorded.  This data is carefully spread
// across a series of objects so that the counts and times can be rapidly
// updated without (usually) having to lock the data, and hence there is usually
// very little contention caused by the tracking.  The data can be viewed via
// the about:profiler URL, with a variety of sorting and filtering choices.
//
// These classes serve as the basis of a profiler of sorts for the Tasks system.
// As a result, design decisions were made to maximize speed, by minimizing
// recurring allocation/deallocation, lock contention and data copying.  In the
// "stable" state, which is reached relatively quickly, there is no separate
// marginal allocation cost associated with construction or destruction of
// tracked objects, no locks are generally employed, and probably the largest
// computational cost is associated with obtaining start and stop times for
// instances as they are created and destroyed.
//
// The following describes the lifecycle of tracking an instance.
//
// First off, when the instance is created, the FROM_HERE macro is expanded
// to specify the birth place (file, line, function) where the instance was
// created.  That data is used to create a transient Location instance
// encapsulating the above triple of information.  The strings (like __FILE__)
// are passed around by reference, with the assumption that they are static, and
// will never go away.  This ensures that the strings can be dealt with as atoms
// with great efficiency (i.e., copying of strings is never needed, and
// comparisons for equality can be based on pointer comparisons).
//
// Next, a Births instance is created for use ONLY on the thread where this
// instance was created.  That Births instance records (in a base class
// BirthOnThread) references to the static data provided in a Location instance,
// as well as a pointer specifying the thread on which the birth takes place.
// Hence there is at most one Births instance for each Location on each thread.
// The derived Births class contains slots for recording statistics about all
// instances born at the same location.  Statistics currently include only the
// count of instances constructed.
//
// Since the base class BirthOnThread contains only constant data, it can be
// freely accessed by any thread at any time (i.e., only the statistic needs to
// be handled carefully, and stats are updated exclusively on the birth thread).
//
// For Tasks, having now either constructed or found the Births instance
// described above, a pointer to the Births instance is then recorded into the
// PendingTask structure in MessageLoop.  This fact alone is very useful in
// debugging, when there is a question of where an instance came from.  In
// addition, the birth time is also recorded and used to later evaluate the
// lifetime duration of the whole Task.  As a result of the above embedding, we
// can find out a Task's location of birth, and thread of birth, without using
// any locks, as all that data is constant across the life of the process.
//
// The above work *could* also be done for any other object as well by calling
// TallyABirthIfActive() and TallyRunOnNamedThreadIfTracking() as appropriate.
//
// The amount of memory used in the above data structures depends on how many
// threads there are, and how many Locations of construction there are.
// Fortunately, we don't use memory that is the product of those two counts, but
// rather we only need one Births instance for each thread that constructs an
// instance at a Location. In many cases, instances are only created on one
// thread, so the memory utilization is actually fairly restrained.
//
// Lastly, when an instance is deleted, the final tallies of statistics are
// carefully accumulated.  That tallying writes into slots (members) in a
// collection of DeathData instances.  For each birth place Location that is
// destroyed on a thread, there is a DeathData instance to record the additional
// death count, as well as accumulate the run-time and queue-time durations for
// the instance as it is destroyed (dies).  By maintaining a single place to
// aggregate this running sum *only* for the given thread, we avoid the need to
// lock such DeathData instances. (i.e., these accumulated stats in a DeathData
// instance are exclusively updated by the singular owning thread).
//
// With the above lifecycle description complete, the major remaining detail is
// explaining how each thread maintains a list of DeathData instances, and of
// Births instances, and is able to avoid additional (redundant/unnecessary)
// allocations.
//
// Each thread maintains a list of data items specific to that thread in a
// ThreadData instance (for that specific thread only).  The two critical items
// are lists of DeathData and Births instances.  These lists are maintained in
// STL maps, which are indexed by Location. As noted earlier, we can compare
// locations very efficiently as we consider the underlying data (file,
// function, line) to be atoms, and hence pointer comparison is used rather than
// (slow) string comparisons.
//
// To provide a mechanism for iterating over all "known threads," which means
// threads that have recorded a birth or a death, we create a singly linked list
// of ThreadData instances. Each such instance maintains a pointer to the next
// one.  A static member of ThreadData provides a pointer to the first item on
// this global list, and access via that all_thread_data_list_head_ item
// requires the use of the list_lock_.
// When new ThreadData instances is added to the global list, it is pre-pended,
// which ensures that any prior acquisition of the list is valid (i.e., the
// holder can iterate over it without fear of it changing, or the necessity of
// using an additional lock.  Iterations are actually pretty rare (used
// primarilly for cleanup, or snapshotting data for display), so this lock has
// very little global performance impact.
//
// The above description tries to define the high performance (run time)
// portions of these classes.  After gathering statistics, calls instigated
// by visiting about:profiler will assemble and aggregate data for display.  The
// following data structures are used for producing such displays.  They are
// not performance critical, and their only major constraint is that they should
// be able to run concurrently with ongoing augmentation of the birth and death
// data.
//
// For a given birth location, information about births is spread across data
// structures that are asynchronously changing on various threads.  For display
// purposes, we need to construct Snapshot instances for each combination of
// birth thread, death thread, and location, along with the count of such
// lifetimes.  We gather such data into a Snapshot instances, so that such
// instances can be sorted and aggregated (and remain frozen during our
// processing).  Snapshot instances use pointers to constant portions of the
// birth and death datastructures, but have local (frozen) copies of the actual
// statistics (birth count, durations, etc. etc.).
//
// A DataCollector is a container object that holds a set of Snapshots. The
// statistics in a snapshot are gathered asynhcronously relative to their
// ongoing updates.  It is possible, though highly unlikely, that stats could be
// incorrectly recorded by this process (all data is held in 32 bit ints, but we
// are not atomically collecting all data, so we could have count that does not,
// for example, match with the number of durations we accumulated).  The
// advantage to having fast (non-atomic) updates of the data outweighs the
// minimal risk of a singular corrupt statistic snapshot (only the snapshot
// could be corrupt, not the underlying and ongoing statistic).  In constrast,
// pointer data that is accessed during snapshotting is completely invariant,
// and hence is perfectly acquired (i.e., no potential corruption, and no risk
// of a bad memory reference).
//
// After an array of Snapshots instances are collected into a DataCollector,
// they need to be prepared for displaying our output.  We currently implement a
// serialization into a Value hierarchy, which is automatically translated to
// JSON when supplied to rendering Java Scirpt.
//
// TODO(jar): We can implement a Snapshot system that *tries* to grab the
// snapshots on the source threads *when* they have MessageLoops available
// (worker threads don't have message loops generally, and hence gathering from
// them will continue to be asynchronous).  We had an implementation of this in
// the past, but the difficulty is dealing with message loops being terminated.
// We can *try* to spam the available threads via some message loop proxy to
// achieve this feat, and it *might* be valuable when we are colecting data for
// upload via UMA (where correctness of data may be more significant than for a
// single screen of about:profiler).
//
// TODO(jar): We should support (optionally) the recording of parent-child
// relationships for tasks.  This should be done by detecting what tasks are
// Born during the running of a parent task.  The resulting data can be used by
// a smarter profiler to aggregate the cost of a series of child tasks into
// the ancestor task.  It can also be used to illuminate what child or parent is
// related to each task.
//
// TODO(jar): We need to store DataCollections, and provide facilities for
// taking the difference between two gathered DataCollections.  For now, we're
// just adding a hack that Reset()s to zero all counts and stats.  This is also
// done in a slighly thread-unsafe fashion, as the resetting is done
// asynchronously relative to ongoing updates (but all data is 32 bit in size).
// For basic profiling, this will work "most of the time," and should be
// sufficient... but storing away DataCollections is the "right way" to do this.
// We'll accomplish this via JavaScript storage of snapshots, and then we'll
// remove the Reset() methods.  We may also need a short-term-max value in
// DeathData that is reset (as synchronously as possible) during each snapshot.
// This will facilitate displaying a max value for each snapshot period.

class MessageLoop;

namespace tracked_objects {

//------------------------------------------------------------------------------
// For a specific thread, and a specific birth place, the collection of all
// death info (with tallies for each death thread, to prevent access conflicts).
class ThreadData;
class BASE_EXPORT BirthOnThread {
 public:
  BirthOnThread(const Location& location, const ThreadData& current);

  const Location location() const;
  const ThreadData* birth_thread() const;

  // Insert our state (location, and thread name) into the dictionary.
  // Use the supplied |prefix| in front of "thread_name" and "location"
  // respectively when defining keys.
  void ToValue(const std::string& prefix,
               base::DictionaryValue* dictionary) const;

 private:
  // File/lineno of birth.  This defines the essence of the task, as the context
  // of the birth (construction) often tell what the item is for.  This field
  // is const, and hence safe to access from any thread.
  const Location location_;

  // The thread that records births into this object.  Only this thread is
  // allowed to update birth_count_ (which changes over time).
  const ThreadData* const birth_thread_;

  DISALLOW_COPY_AND_ASSIGN(BirthOnThread);
};

//------------------------------------------------------------------------------
// A class for accumulating counts of births (without bothering with a map<>).

class BASE_EXPORT Births: public BirthOnThread {
 public:
  Births(const Location& location, const ThreadData& current);

  int birth_count() const;

  // When we have a birth we update the count for this BirhPLace.
  void RecordBirth();

  // When a birthplace is changed (updated), we need to decrement the counter
  // for the old instance.
  void ForgetBirth();

  // Hack to quickly reset all counts to zero.
  void Clear();

 private:
  // The number of births on this thread for our location_.
  int birth_count_;

  DISALLOW_COPY_AND_ASSIGN(Births);
};

//------------------------------------------------------------------------------
// Basic info summarizing multiple destructions of a tracked object with a
// single birthplace (fixed Location).  Used both on specific threads, and also
// in snapshots when integrating assembled data.

class BASE_EXPORT DeathData {
 public:
  // Default initializer.
  DeathData();

  // When deaths have not yet taken place, and we gather data from all the
  // threads, we create DeathData stats that tally the number of births without
  // a corresponding death.
  explicit DeathData(int count);

  // Update stats for a task destruction (death) that had a Run() time of
  // |duration|, and has had a queueing delay of |queue_duration|.
  void RecordDeath(const DurationInt queue_duration,
                   const DurationInt run_duration,
                   int random_number);

  // Metrics accessors, used only in tests.
  int count() const;
  DurationInt run_duration_sum() const;
  DurationInt run_duration_max() const;
  DurationInt run_duration_sample() const;
  DurationInt queue_duration_sum() const;
  DurationInt queue_duration_max() const;
  DurationInt queue_duration_sample() const;

  // Construct a DictionaryValue instance containing all our stats. The caller
  // assumes ownership of the returned instance.
  base::DictionaryValue* ToValue() const;

  // Reset the max values to zero.
  void ResetMax();

  // Reset all tallies to zero. This is used as a hack on realtime data.
  void Clear();

 private:
  // Members are ordered from most regularly read and updated, to least
  // frequently used.  This might help a bit with cache lines.
  // Number of runs seen (divisor for calculating averages).
  int count_;
  // Basic tallies, used to compute averages.
  DurationInt run_duration_sum_;
  DurationInt queue_duration_sum_;
  // Max values, used by local visualization routines.  These are often read,
  // but rarely updated.
  DurationInt run_duration_max_;
  DurationInt queue_duration_max_;
  // Samples, used by by crowd sourcing gatherers.  These are almost never read,
  // and rarely updated.
  DurationInt run_duration_sample_;
  DurationInt queue_duration_sample_;
};

//------------------------------------------------------------------------------
// A temporary collection of data that can be sorted and summarized.  It is
// gathered (carefully) from many threads.  Instances are held in arrays and
// processed, filtered, and rendered.
// The source of this data was collected on many threads, and is asynchronously
// changing.  The data in this instance is not asynchronously changing.

class BASE_EXPORT Snapshot {
 public:
  // When snapshotting a full life cycle set (birth-to-death), use this:
  Snapshot(const BirthOnThread& birth_on_thread,
           const ThreadData& death_thread,
           const DeathData& death_data);

  // When snapshotting a birth, with no death yet, use this:
  Snapshot(const BirthOnThread& birth_on_thread, int count);

  // Accessor, that provides default value when there is no death thread.
  const std::string DeathThreadName() const;

  // Construct a DictionaryValue instance containing all our data recursively.
  // The caller assumes ownership of the memory in the returned instance.
  base::DictionaryValue* ToValue() const;

 private:
  const BirthOnThread* birth_;  // Includes Location and birth_thread.
  const ThreadData* death_thread_;
  DeathData death_data_;
};

//------------------------------------------------------------------------------
// For each thread, we have a ThreadData that stores all tracking info generated
// on this thread.  This prevents the need for locking as data accumulates.
// We use ThreadLocalStorage to quickly identfy the current ThreadData context.
// We also have a linked list of ThreadData instances, and that list is used to
// harvest data from all existing instances.

class BASE_EXPORT ThreadData {
 public:
  // Current allowable states of the tracking system.  The states can vary
  // between ACTIVE and DEACTIVATED, but can never go back to UNINITIALIZED.
  enum Status {
    UNINITIALIZED,              // PRistine, link-time state before running.
    DORMANT_DURING_TESTS,       // Only used during testing.
    DEACTIVATED,                // No longer recording profling.
    PROFILING_ACTIVE,           // Recording profiles (no parent-child links).
    PROFILING_CHILDREN_ACTIVE,  // Fully active, recording parent-child links.
  };

  typedef std::map<Location, Births*> BirthMap;
  typedef std::map<const Births*, DeathData> DeathMap;
  typedef std::pair<const Births*, const Births*> ParentChildPair;
  typedef std::set<ParentChildPair> ParentChildSet;
  typedef std::stack<const Births*> ParentStack;

  // Initialize the current thread context with a new instance of ThreadData.
  // This is used by all threads that have names, and should be explicitly
  // set *before* any births on the threads have taken place.  It is generally
  // only used by the message loop, which has a well defined thread name.
  static void InitializeThreadContext(const std::string& suggested_name);

  // Using Thread Local Store, find the current instance for collecting data.
  // If an instance does not exist, construct one (and remember it for use on
  // this thread.
  // This may return NULL if the system is disabled for any reason.
  static ThreadData* Get();

  // Constructs a DictionaryValue instance containing all recursive results in
  // our process.  The caller assumes ownership of the memory in the returned
  // instance.  During the scavenging, if |reset_max| is true, then the
  // DeathData instances max-values are reset to zero during this scan.
  static base::DictionaryValue* ToValue(bool reset_max);

  // Finds (or creates) a place to count births from the given location in this
  // thread, and increment that tally.
  // TallyABirthIfActive will returns NULL if the birth cannot be tallied.
  static Births* TallyABirthIfActive(const Location& location);

  // Records the end of a timed run of an object.  The |completed_task| contains
  // a pointer to a Births, the time_posted, and a delayed_start_time if any.
  // The |start_of_run| indicates when we started to perform the run of the
  // task.  The delayed_start_time is non-null for tasks that were posted as
  // delayed tasks, and it indicates when the task should have run (i.e., when
  // it should have posted out of the timer queue, and into the work queue.
  // The |end_of_run| was just obtained by a call to Now() (just after the task
  // finished). It is provided as an argument to help with testing.
  static void TallyRunOnNamedThreadIfTracking(
      const base::TrackingInfo& completed_task,
      const TrackedTime& start_of_run,
      const TrackedTime& end_of_run);

  // Record the end of a timed run of an object.  The |birth| is the record for
  // the instance, the |time_posted| records that instant, which is presumed to
  // be when the task was posted into a queue to run on a worker thread.
  // The |start_of_run| is when the worker thread started to perform the run of
  // the task.
  // The |end_of_run| was just obtained by a call to Now() (just after the task
  // finished).
  static void TallyRunOnWorkerThreadIfTracking(
      const Births* birth,
      const TrackedTime& time_posted,
      const TrackedTime& start_of_run,
      const TrackedTime& end_of_run);

  // Record the end of execution in region, generally corresponding to a scope
  // being exited.
  static void TallyRunInAScopedRegionIfTracking(
      const Births* birth,
      const TrackedTime& start_of_run,
      const TrackedTime& end_of_run);

  const std::string thread_name() const;

  // Snapshot (under a lock) copies of the maps in each ThreadData instance. For
  // each set of maps (BirthMap, DeathMap, and ParentChildSet) call the Append()
  // method of the |target| DataCollector.  If |reset_max| is true, then the max
  // values in each DeathData instance should be reset during the scan.
  static void SendAllMaps(bool reset_max, class DataCollector* target);

  // Hack: asynchronously clear all birth counts and death tallies data values
  // in all ThreadData instances.  The numerical (zeroing) part is done without
  // use of a locks or atomics exchanges, and may (for int64 values) produce
  // bogus counts VERY rarely.
  static void ResetAllThreadData();

  // Initializes all statics if needed (this initialization call should be made
  // while we are single threaded). Returns false if unable to initialize.
  static bool Initialize();

  // Sets internal status_.
  // If |status| is false, then status_ is set to DEACTIVATED.
  // If |status| is true, then status_ is set to, PROFILING_ACTIVE, or
  // PROFILING_CHILDREN_ACTIVE.
  // If tracking is not compiled in, this function will return false.
  // If parent-child tracking is not compiled in, then an attempt to set the
  // status to PROFILING_CHILDREN_ACTIVE will only result in a status of
  // PROFILING_ACTIVE (i.e., it can't be set to a higher level than what is
  // compiled into the binary, and parent-child tracking at the
  // PROFILING_CHILDREN_ACTIVE level might not be compiled in).
  static bool InitializeAndSetTrackingStatus(bool status);

  // Indicate if any sort of profiling is being done (i.e., we are more than
  // DEACTIVATED).
  static bool tracking_status();

  // For testing only, indicate if the status of parent-child tracking is turned
  // on.  This is currently a compiled option, atop tracking_status().
  static bool tracking_parent_child_status();

  // Special versions of Now() for getting times at start and end of a tracked
  // run.  They are super fast when tracking is disabled, and have some internal
  // side effects when we are tracking, so that we can deduce the amount of time
  // accumulated outside of execution of tracked runs.
  // The task that will be tracked is passed in as |parent| so that parent-child
  // relationships can be (optionally) calculated.
  static TrackedTime NowForStartOfRun(const Births* parent);
  static TrackedTime NowForEndOfRun();

  // Provide a time function that does nothing (runs fast) when we don't have
  // the profiler enabled.  It will generally be optimized away when it is
  // ifdef'ed to be small enough (allowing the profiler to be "compiled out" of
  // the code).
  static TrackedTime Now();

  // This function can be called at process termination to validate that thread
  // cleanup routines have been called for at least some number of named
  // threads.
  static void EnsureCleanupWasCalled(int major_threads_shutdown_count);

 private:
  // Allow only tests to call ShutdownSingleThreadedCleanup.  We NEVER call it
  // in production code.
  // TODO(jar): Make this a friend in DEBUG only, so that the optimizer has a
  // better change of optimizing (inlining? etc.) private methods (knowing that
  // there will be no need for an external entry point).
  friend class TrackedObjectsTest;
  FRIEND_TEST_ALL_PREFIXES(TrackedObjectsTest, MinimalStartupShutdown);
  FRIEND_TEST_ALL_PREFIXES(TrackedObjectsTest, TinyStartupShutdown);
  FRIEND_TEST_ALL_PREFIXES(TrackedObjectsTest, ParentChildTest);

  // Worker thread construction creates a name since there is none.
  explicit ThreadData(int thread_number);

  // Message loop based construction should provide a name.
  explicit ThreadData(const std::string& suggested_name);

  ~ThreadData();

  // Push this instance to the head of all_thread_data_list_head_, linking it to
  // the previous head.  This is performed after each construction, and leaves
  // the instance permanently on that list.
  void PushToHeadOfList();

  // (Thread safe) Get start of list of all ThreadData instances using the lock.
  static ThreadData* first();

  // Iterate through the null terminated list of ThreadData instances.
  ThreadData* next() const;


  // In this thread's data, record a new birth.
  Births* TallyABirth(const Location& location);

  // Find a place to record a death on this thread.
  void TallyADeath(const Births& birth,
                   DurationInt queue_duration,
                   DurationInt duration);

  // Using our lock, make a copy of the specified maps.  This call may be made
  // on  non-local threads, which necessitate the use of the lock to prevent
  // the map(s) from being reallocaed while they are copied. If |reset_max| is
  // true, then, just after we copy the DeathMap, we will set the max values to
  // zero in the active DeathMap (not the snapshot).
  void SnapshotMaps(bool reset_max,
                    BirthMap* birth_map,
                    DeathMap* death_map,
                    ParentChildSet* parent_child_set);

  // Using our lock to protect the iteration, Clear all birth and death data.
  void Reset();

  // This method is called by the TLS system when a thread terminates.
  // The argument may be NULL if this thread has never tracked a birth or death.
  static void OnThreadTermination(void* thread_data);

  // This method should be called when a worker thread terminates, so that we
  // can save all the thread data into a cache of reusable ThreadData instances.
  void OnThreadTerminationCleanup();

  // Cleans up data structures, and returns statics to near pristine (mostly
  // uninitialized) state.  If there is any chance that other threads are still
  // using the data structures, then the |leak| argument should be passed in as
  // true, and the data structures (birth maps, death maps, ThreadData
  // insntances, etc.) will be leaked and not deleted.  If you have joined all
  // threads since the time that InitializeAndSetTrackingStatus() was called,
  // then you can pass in a |leak| value of false, and this function will
  // delete recursively all data structures, starting with the list of
  // ThreadData instances.
  static void ShutdownSingleThreadedCleanup(bool leak);

  // We use thread local store to identify which ThreadData to interact with.
  static base::ThreadLocalStorage::Slot tls_index_;

  // List of ThreadData instances for use with worker threads. When a worker
  // thread is done (terminated), we push it onto this llist.  When a new worker
  // thread is created, we first try to re-use a ThreadData instance from the
  // list, and if none are available, construct a new one.
  // This is only accessed while list_lock_ is held.
  static ThreadData* first_retired_worker_;

  // Link to the most recently created instance (starts a null terminated list).
  // The list is traversed by about:profiler when it needs to snapshot data.
  // This is only accessed while list_lock_ is held.
  static ThreadData* all_thread_data_list_head_;

  // The next available worker thread number.  This should only be accessed when
  // the list_lock_ is held.
  static int worker_thread_data_creation_count_;

  // The number of times TLS has called us back to cleanup a ThreadData
  // instance. This is only accessed while list_lock_ is held.
  static int cleanup_count_;

  // Incarnation sequence number, indicating how many times (during unittests)
  // we've either transitioned out of UNINITIALIZED, or into that state.  This
  // value is only accessed while the list_lock_ is held.
  static int incarnation_counter_;

  // Protection for access to all_thread_data_list_head_, and to
  // unregistered_thread_data_pool_.  This lock is leaked at shutdown.
  // The lock is very infrequently used, so we can afford to just make a lazy
  // instance and be safe.
  static base::LazyInstance<base::Lock>::Leaky list_lock_;

  // We set status_ to SHUTDOWN when we shut down the tracking service.
  static Status status_;

  // Link to next instance (null terminated list). Used to globally track all
  // registered instances (corresponds to all registered threads where we keep
  // data).
  ThreadData* next_;

  // Pointer to another ThreadData instance for a Worker-Thread that has been
  // retired (its thread was terminated).  This value is non-NULL only for a
  // retired ThreadData associated with a Worker-Thread.
  ThreadData* next_retired_worker_;

  // The name of the thread that is being recorded.  If this thread has no
  // message_loop, then this is a worker thread, with a sequence number postfix.
  std::string thread_name_;

  // Indicate if this is a worker thread, and the ThreadData contexts should be
  // stored in the unregistered_thread_data_pool_ when not in use.
  // Value is zero when it is not a worker thread.  Value is a positive integer
  // corresponding to the created thread name if it is a worker thread.
  int worker_thread_number_;

  // A map used on each thread to keep track of Births on this thread.
  // This map should only be accessed on the thread it was constructed on.
  // When a snapshot is needed, this structure can be locked in place for the
  // duration of the snapshotting activity.
  BirthMap birth_map_;

  // Similar to birth_map_, this records informations about death of tracked
  // instances (i.e., when a tracked instance was destroyed on this thread).
  // It is locked before changing, and hence other threads may access it by
  // locking before reading it.
  DeathMap death_map_;

  // A set of parents that created children tasks on this thread. Each pair
  // corresponds to potentially non-local Births (location and thread), and a
  // local Births (that took place on this thread).
  ParentChildSet parent_child_set_;

  // Lock to protect *some* access to BirthMap and DeathMap.  The maps are
  // regularly read and written on this thread, but may only be read from other
  // threads.  To support this, we acquire this lock if we are writing from this
  // thread, or reading from another thread.  For reading from this thread we
  // don't need a lock, as there is no potential for a conflict since the
  // writing is only done from this thread.
  mutable base::Lock map_lock_;

  // The stack of parents that are currently being profiled. This includes only
  // tasks that have started a timer recently via NowForStartOfRun(), but not
  // yet concluded with a NowForEndOfRun().  Usually this stack is one deep, but
  // if a scoped region is profiled, or <sigh> a task runs a nested-message
  // loop, then the stack can grow larger.  Note that we don't try to deduct
  // time in nested porfiles, as our current timer is based on wall-clock time,
  // and not CPU time (and we're hopeful that nested timing won't be a
  // significant additional cost).
  ParentStack parent_stack_;

  // A random number that we used to select decide which sample to keep as a
  // representative sample in each DeathData instance.  We can't start off with
  // much randomness (because we can't call RandInt() on all our threads), so
  // we stir in more and more as we go.
  int32 random_number_;

  // Record of what the incarnation_counter_ was when this instance was created.
  // If the incarnation_counter_ has changed, then we avoid pushing into the
  // pool (this is only critical in tests which go through multiple
  // incarnations).
  int incarnation_count_for_pool_;

  DISALLOW_COPY_AND_ASSIGN(ThreadData);
};

//------------------------------------------------------------------------------
// DataCollector is a container class for Snapshot and BirthOnThread count
// items.

class BASE_EXPORT DataCollector {
 public:
  typedef std::vector<Snapshot> Collection;

  // Construct with a list of how many threads should contribute.  This helps us
  // determine (in the async case) when we are done with all contributions.
  DataCollector();
  ~DataCollector();

  // Adds all stats from the indicated thread into our arrays.  Accepts copies
  // of the birth_map and death_map, so that the data will not change during the
  // iterations and processing.
  void Append(const ThreadData &thread_data,
              const ThreadData::BirthMap& birth_map,
              const ThreadData::DeathMap& death_map,
              const ThreadData::ParentChildSet& parent_child_set);

  // After the accumulation phase, the following accessor is used to process the
  // data (i.e., sort it, filter it, etc.).
  Collection* collection();

  // Adds entries for all the remaining living objects (objects that have
  // tallied a birth, but have not yet tallied a matching death, and hence must
  // be either running, queued up, or being held in limbo for future posting).
  // This should be called after all known ThreadData instances have been
  // processed using Append().
  void AddListOfLivingObjects();

  // Generates a ListValue representation of the vector of snapshots, and
  // inserts the results into |dictionary|.
  void ToValue(base::DictionaryValue* dictionary) const;

 private:
  typedef std::map<const BirthOnThread*, int> BirthCount;

  // The array that we collect data into.
  Collection collection_;

  // The total number of births recorded at each location for which we have not
  // seen a death count.  This map changes as we do Append() calls, and is later
  // used by AddListOfLivingObjects() to gather up unaccounted for births.
  BirthCount global_birth_count_;

  // The complete list of parent-child relationships among tasks.
  ThreadData::ParentChildSet parent_child_set_;

  DISALLOW_COPY_AND_ASSIGN(DataCollector);
};

//------------------------------------------------------------------------------
// Provide simple way to to start global tracking, and to tear down tracking
// when done.  The design has evolved to *not* do any teardown (and just leak
// all allocated data structures).  As a result, we don't have any code in this
// destructor, and perhaps this whole class should go away.

class BASE_EXPORT AutoTracking {
 public:
  AutoTracking() {
    ThreadData::Initialize();
  }

  ~AutoTracking() {
    // TODO(jar): Consider emitting a CSV dump of the data at this point.  This
    // should be called after the message loops have all terminated (or at least
    // the main message loop is gone), so there is little chance for additional
    // tasks to be Run.
  }

 private:

  DISALLOW_COPY_AND_ASSIGN(AutoTracking);
};

}  // namespace tracked_objects

#endif  // BASE_TRACKED_OBJECTS_H_
