// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains utility functions and classes that help the
// implementation, and management of the Callback objects.

#ifndef BASE_CALLBACK_INTERNAL_H_
#define BASE_CALLBACK_INTERNAL_H_
#pragma once

#include <stddef.h>

#include "base/base_export.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"

namespace base {
namespace internal {

// BindStateBase is used to provide an opaque handle that the Callback
// class can use to represent a function object with bound arguments.  It
// behaves as an existential type that is used by a corresponding
// DoInvoke function to perform the function execution.  This allows
// us to shield the Callback class from the types of the bound argument via
// "type erasure."
class BindStateBase : public RefCountedThreadSafe<BindStateBase> {
 protected:
  friend class RefCountedThreadSafe<BindStateBase>;
  virtual ~BindStateBase() {}
};

// Holds the Callback methods that don't require specialization to reduce
// template bloat.
class BASE_EXPORT CallbackBase {
 public:
  // Returns true if Callback is null (doesn't refer to anything).
  bool is_null() const;

  // Returns the Callback into an uninitialized state.
  void Reset();

 protected:
  // In C++, it is safe to cast function pointers to function pointers of
  // another type. It is not okay to use void*. We create a InvokeFuncStorage
  // that that can store our function pointer, and then cast it back to
  // the original type on usage.
  typedef void(*InvokeFuncStorage)(void);

  // Returns true if this callback equals |other|. |other| may be null.
  bool Equals(const CallbackBase& other) const;

  // Allow initializing of |bind_state_| via the constructor to avoid default
  // initialization of the scoped_refptr.  We do not also initialize
  // |polymorphic_invoke_| here because doing a normal assignment in the
  // derived Callback templates makes for much nicer compiler errors.
  explicit CallbackBase(BindStateBase* bind_state);

  // Force the destructor to be instantiated inside this translation unit so
  // that our subclasses will not get inlined versions.  Avoids more template
  // bloat.
  ~CallbackBase();

  scoped_refptr<BindStateBase> bind_state_;
  InvokeFuncStorage polymorphic_invoke_;
};

// This is a typetraits object that's used to take an argument type, and
// extract a suitable type for storing and forwarding arguments.
//
// In particular, it strips off references, and converts arrays to
// pointers for storage; and it avoids accidentally trying to create a
// "reference of a reference" if the argument is a reference type.
//
// This array type becomes an issue for storage because we are passing bound
// parameters by const reference. In this case, we end up passing an actual
// array type in the initializer list which C++ does not allow.  This will
// break passing of C-string literals.
template <typename T>
struct CallbackParamTraits {
  typedef const T& ForwardType;
  typedef T StorageType;
};

// The Storage should almost be impossible to trigger unless someone manually
// specifies type of the bind parameters.  However, in case they do,
// this will guard against us accidentally storing a reference parameter.
//
// The ForwardType should only be used for unbound arguments.
template <typename T>
struct CallbackParamTraits<T&> {
  typedef T& ForwardType;
  typedef T StorageType;
};

// Note that for array types, we implicitly add a const in the conversion. This
// means that it is not possible to bind array arguments to functions that take
// a non-const pointer. Trying to specialize the template based on a "const
// T[n]" does not seem to match correctly, so we are stuck with this
// restriction.
template <typename T, size_t n>
struct CallbackParamTraits<T[n]> {
  typedef const T* ForwardType;
  typedef const T* StorageType;
};

// See comment for CallbackParamTraits<T[n]>.
template <typename T>
struct CallbackParamTraits<T[]> {
  typedef const T* ForwardType;
  typedef const T* StorageType;
};

// Parameter traits for movable-but-not-copyable scopers.
//
// Callback<>/Bind() understands movable-but-not-copyable semantics where
// the type cannot be copied but can still have its state destructively
// transferred (aka. moved) to another instance of the same type by calling a
// helper function.  When used with Bind(), this signifies transferal of the
// object's state to the target function.
//
// For these types, the ForwardType must not be a const reference, or a
// reference.  A const reference is inappropriate, and would break const
// correctness, because we are implementing a destructive move.  A non-const
// reference cannot be used with temporaries which means the result of a
// function or a cast would not be usable with Callback<> or Bind().
//
// TODO(ajwong): We might be able to use SFINAE to search for the existence of
// a Pass() function in the type and avoid the whitelist in CallbackParamTraits
// and CallbackForward.
template <typename T>
struct CallbackParamTraits<scoped_ptr<T> > {
  typedef scoped_ptr<T> ForwardType;
  typedef scoped_ptr<T> StorageType;
};

template <typename T>
struct CallbackParamTraits<scoped_array<T> > {
  typedef scoped_array<T> ForwardType;
  typedef scoped_array<T> StorageType;
};

template <typename T>
struct CallbackParamTraits<scoped_ptr_malloc<T> > {
  typedef scoped_ptr_malloc<T> ForwardType;
  typedef scoped_ptr_malloc<T> StorageType;
};

// CallbackForward() is a very limited simulation of C++11's std::forward()
// used by the Callback/Bind system for a set of movable-but-not-copyable
// types.  It is needed because forwarding a movable-but-not-copyable
// argument to another function requires us to invoke the proper move
// operator to create a rvalue version of the type.  The supported types are
// whitelisted below as overloads of the CallbackForward() function. The
// default template compiles out to be a no-op.
//
// In C++11, std::forward would replace all uses of this function.  However, it
// is impossible to implement a general std::forward with C++11 due to a lack
// of rvalue references.
template <typename T>
T& CallbackForward(T& t) { return t; }

template <typename T>
scoped_ptr<T> CallbackForward(scoped_ptr<T>& p) { return p.Pass(); }

template <typename T>
scoped_ptr<T> CallbackForward(scoped_array<T>& p) { return p.Pass(); }

template <typename T>
scoped_ptr<T> CallbackForward(scoped_ptr_malloc<T>& p) { return p.Pass(); }

}  // namespace internal
}  // namespace base

#endif  // BASE_CALLBACK_INTERNAL_H_
