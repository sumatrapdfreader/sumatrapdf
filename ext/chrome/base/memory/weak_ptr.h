// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Weak pointers help in cases where you have many objects referring back to a
// shared object and you wish for the lifetime of the shared object to not be
// bound to the lifetime of the referrers.  In other words, this is useful when
// reference counting is not a good fit.
//
// Thread-safety notes:
// When you get a WeakPtr (from a WeakPtrFactory or SupportsWeakPtr),
// the WeakPtr becomes bound to the current thread. You may only
// dereference the WeakPtr on that thread. However, it is safe to
// destroy the WeakPtr object on another thread.
// Since a WeakPtr object may be destroyed on a background thread,
// querying WeakPtrFactory's HasWeakPtrs() method can be racy.
//
//
// A common alternative to weak pointers is to have the shared object hold a
// list of all referrers, and then when the shared object is destroyed, it
// calls a method on the referrers to tell them to drop their references.  This
// approach also requires the referrers to tell the shared object when they get
// destroyed so that the shared object can remove the referrer from its list of
// referrers.  Such a solution works, but it is a bit complex.
//
// EXAMPLE:
//
//  class Controller : public SupportsWeakPtr<Controller> {
//   public:
//    void SpawnWorker() { Worker::StartNew(AsWeakPtr()); }
//    void WorkComplete(const Result& result) { ... }
//  };
//
//  class Worker {
//   public:
//    static void StartNew(const WeakPtr<Controller>& controller) {
//      Worker* worker = new Worker(controller);
//      // Kick off asynchronous processing...
//    }
//   private:
//    Worker(const WeakPtr<Controller>& controller)
//        : controller_(controller) {}
//    void DidCompleteAsynchronousProcessing(const Result& result) {
//      if (controller_)
//        controller_->WorkComplete(result);
//    }
//    WeakPtr<Controller> controller_;
//  };
//
// Given the above classes, a consumer may allocate a Controller object, call
// SpawnWorker several times, and then destroy the Controller object before all
// of the workers have completed.  Because the Worker class only holds a weak
// pointer to the Controller, we don't have to worry about the Worker
// dereferencing the Controller back pointer after the Controller has been
// destroyed.
//

#ifndef BASE_MEMORY_WEAK_PTR_H_
#define BASE_MEMORY_WEAK_PTR_H_
#pragma once

#include "base/base_export.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"

namespace base {

namespace internal {
// These classes are part of the WeakPtr implementation.
// DO NOT USE THESE CLASSES DIRECTLY YOURSELF.

class BASE_EXPORT WeakReference {
 public:
  // While Flag is bound to a specific thread, it may be deleted from another
  // via base::WeakPtr::~WeakPtr().
  class Flag : public RefCountedThreadSafe<Flag> {
   public:
    Flag();

    void Invalidate();
    bool IsValid() const;

    void DetachFromThread() { thread_checker_.DetachFromThread(); }

   private:
    friend class base::RefCountedThreadSafe<Flag>;

    ~Flag();

    ThreadChecker thread_checker_;
    bool is_valid_;
  };

  WeakReference();
  explicit WeakReference(const Flag* flag);
  ~WeakReference();

  bool is_valid() const;

 private:
  scoped_refptr<const Flag> flag_;
};

class BASE_EXPORT WeakReferenceOwner {
 public:
  WeakReferenceOwner();
  ~WeakReferenceOwner();

  WeakReference GetRef() const;

  bool HasRefs() const {
    return flag_.get() && !flag_->HasOneRef();
  }

  void Invalidate();

  // Indicates that this object will be used on another thread from now on.
  void DetachFromThread() {
    if (flag_) flag_->DetachFromThread();
  }

 private:
  mutable scoped_refptr<WeakReference::Flag> flag_;
};

// This class simplifies the implementation of WeakPtr's type conversion
// constructor by avoiding the need for a public accessor for ref_.  A
// WeakPtr<T> cannot access the private members of WeakPtr<U>, so this
// base class gives us a way to access ref_ in a protected fashion.
class BASE_EXPORT WeakPtrBase {
 public:
  WeakPtrBase();
  ~WeakPtrBase();

 protected:
  WeakPtrBase(const WeakReference& ref);

  WeakReference ref_;
};

}  // namespace internal

template <typename T> class SupportsWeakPtr;
template <typename T> class WeakPtrFactory;

// The WeakPtr class holds a weak reference to |T*|.
//
// This class is designed to be used like a normal pointer.  You should always
// null-test an object of this class before using it or invoking a method that
// may result in the underlying object being destroyed.
//
// EXAMPLE:
//
//   class Foo { ... };
//   WeakPtr<Foo> foo;
//   if (foo)
//     foo->method();
//
template <typename T>
class WeakPtr : public internal::WeakPtrBase {
 public:
  WeakPtr() : ptr_(NULL) {
  }

  // Allow conversion from U to T provided U "is a" T.
  template <typename U>
  WeakPtr(const WeakPtr<U>& other) : WeakPtrBase(other), ptr_(other.get()) {
  }

  T* get() const { return ref_.is_valid() ? ptr_ : NULL; }
  operator T*() const { return get(); }

  T* operator*() const {
    DCHECK(get() != NULL);
    return *get();
  }
  T* operator->() const {
    DCHECK(get() != NULL);
    return get();
  }

  void reset() {
    ref_ = internal::WeakReference();
    ptr_ = NULL;
  }

 private:
  friend class SupportsWeakPtr<T>;
  friend class WeakPtrFactory<T>;

  WeakPtr(const internal::WeakReference& ref, T* ptr)
      : WeakPtrBase(ref), ptr_(ptr) {
  }

  // This pointer is only valid when ref_.is_valid() is true.  Otherwise, its
  // value is undefined (as opposed to NULL).
  T* ptr_;
};

// A class may extend from SupportsWeakPtr to expose weak pointers to itself.
// This is useful in cases where you want others to be able to get a weak
// pointer to your class.  It also has the property that you don't need to
// initialize it from your constructor.
template <class T>
class SupportsWeakPtr {
 public:
  SupportsWeakPtr() {}

  WeakPtr<T> AsWeakPtr() {
    return WeakPtr<T>(weak_reference_owner_.GetRef(), static_cast<T*>(this));
  }

  // Indicates that this object will be used on another thread from now on.
  void DetachFromThread() {
    weak_reference_owner_.DetachFromThread();
  }

 private:
  internal::WeakReferenceOwner weak_reference_owner_;
  DISALLOW_COPY_AND_ASSIGN(SupportsWeakPtr);
};

// A class may alternatively be composed of a WeakPtrFactory and thereby
// control how it exposes weak pointers to itself.  This is helpful if you only
// need weak pointers within the implementation of a class.  This class is also
// useful when working with primitive types.  For example, you could have a
// WeakPtrFactory<bool> that is used to pass around a weak reference to a bool.
template <class T>
class WeakPtrFactory {
 public:
  explicit WeakPtrFactory(T* ptr) : ptr_(ptr) {
  }

  WeakPtr<T> GetWeakPtr() {
    return WeakPtr<T>(weak_reference_owner_.GetRef(), ptr_);
  }

  // Call this method to invalidate all existing weak pointers.
  void InvalidateWeakPtrs() {
    weak_reference_owner_.Invalidate();
  }

  // Call this method to determine if any weak pointers exist.
  bool HasWeakPtrs() const {
    return weak_reference_owner_.HasRefs();
  }

  // Indicates that this object will be used on another thread from now on.
  void DetachFromThread() {
    weak_reference_owner_.DetachFromThread();
  }

 private:
  internal::WeakReferenceOwner weak_reference_owner_;
  T* ptr_;
  DISALLOW_IMPLICIT_CONSTRUCTORS(WeakPtrFactory);
};

}  // namespace base

#endif  // BASE_MEMORY_WEAK_PTR_H_
