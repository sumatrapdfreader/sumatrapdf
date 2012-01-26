// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MESSAGE_LOOP_HELPERS_H_
#define BASE_MESSAGE_LOOP_HELPERS_H_
#pragma once

#include "base/basictypes.h"

namespace tracked_objects {
class Location;
}

namespace base {

namespace subtle {
template <class T, class R> class DeleteHelperInternal;
template <class T, class R> class ReleaseHelperInternal;
}

// Template helpers which use a function indirection to erase T from the
// function signature while still remembering it so we can call the correct
// destructor/release function.
// We use this trick so we don't need to include bind.h in a header file like
// message_loop.h. We also wrap the helpers in a templated class to make it
// easier for users of DeleteSoon to declare the helper as a friend.
template <class T>
class DeleteHelper {
 private:
  template <class T2, class R> friend class subtle::DeleteHelperInternal;

  static void DoDelete(const void* object) {
    delete reinterpret_cast<const T*>(object);
  }

  DISALLOW_COPY_AND_ASSIGN(DeleteHelper);
};

template <class T>
class ReleaseHelper {
 private:
  template <class T2, class R> friend class subtle::ReleaseHelperInternal;

  static void DoRelease(const void* object) {
    reinterpret_cast<const T*>(object)->Release();
  }

  DISALLOW_COPY_AND_ASSIGN(ReleaseHelper);
};

namespace subtle {

// An internal MessageLoop-like class helper for DeleteHelper and ReleaseHelper.
// We don't want to expose the Do*() functions directly directly since the void*
// argument makes it possible to pass/ an object of the wrong type to delete.
// Instead, we force callers to go through these internal helpers for type
// safety. MessageLoop-like classes which expose DeleteSoon or ReleaseSoon
// methods should friend the appropriate helper and implement a corresponding
// *Internal method with the following signature:
// bool(const tracked_objects::Location&,
//      void(*function)(const void*),
//      void* object)
// An implementation of this function should simply create a base::Closure
// from (function, object) and return the result of posting the task.
template <class T, class ReturnType>
class DeleteHelperInternal {
 public:
  template <class MessageLoopType>
  static ReturnType DeleteOnMessageLoop(
      MessageLoopType* message_loop,
      const tracked_objects::Location& from_here,
      const T* object) {
    return message_loop->DeleteSoonInternal(from_here,
                                            &DeleteHelper<T>::DoDelete,
                                            object);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DeleteHelperInternal);
};

template <class T, class ReturnType>
class ReleaseHelperInternal {
 public:
  template <class MessageLoopType>
  static ReturnType ReleaseOnMessageLoop(
      MessageLoopType* message_loop,
      const tracked_objects::Location& from_here,
      const T* object) {
    return message_loop->ReleaseSoonInternal(from_here,
                                             &ReleaseHelper<T>::DoRelease,
                                             object);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ReleaseHelperInternal);
};

}  // namespace subtle

}  // namespace base

#endif  // BASE_MESSAGE_LOOP_HELPERS_H_
