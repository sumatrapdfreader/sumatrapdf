// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MESSAGE_LOOP_PROXY_H_
#define BASE_MESSAGE_LOOP_PROXY_H_
#pragma once

#include "base/base_export.h"
#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop_helpers.h"

namespace tracked_objects {
class Location;
} // namespace tracked_objects

namespace base {

struct MessageLoopProxyTraits;

// This class provides a thread-safe refcounted interface to the Post* methods
// of a message loop. This class can outlive the target message loop.
// MessageLoopProxy objects are constructed automatically for all MessageLoops.
// So, to access them, you can use any of the following:
//   Thread::message_loop_proxy()
//   MessageLoop::current()->message_loop_proxy()
//   MessageLoopProxy::current()
class BASE_EXPORT MessageLoopProxy
    : public base::RefCountedThreadSafe<MessageLoopProxy,
                                        MessageLoopProxyTraits> {
 public:
  // These methods are the same as in message_loop.h, but are guaranteed to
  // either post the Task to the MessageLoop (if it's still alive), or the task
  // is discarded.
  // They return true iff the thread existed and the task was posted.  Note that
  // even if the task is posted, there's no guarantee that it will run; for
  // example the target loop may already be quitting, or in the case of a
  // delayed task a Quit message may preempt it in the message loop queue.
  // Conversely, a return value of false is a guarantee the task will not run.
  virtual bool PostTask(const tracked_objects::Location& from_here,
                        const base::Closure& task) = 0;
  virtual bool PostDelayedTask(const tracked_objects::Location& from_here,
                               const base::Closure& task,
                               int64 delay_ms) = 0;
  virtual bool PostNonNestableTask(const tracked_objects::Location& from_here,
                                   const base::Closure& task) = 0;
  virtual bool PostNonNestableDelayedTask(
      const tracked_objects::Location& from_here,
      const base::Closure& task,
      int64 delay_ms) = 0;

  // A method which checks if the caller is currently running in the thread that
  // this proxy represents.
  virtual bool BelongsToCurrentThread() = 0;

  // Executes |task| on the given MessageLoopProxy.  On completion, |reply|
  // is passed back to the MessageLoopProxy for the thread that called
  // PostTaskAndReply().  Both |task| and |reply| are guaranteed to be deleted
  // on the thread from which PostTaskAndReply() is invoked.  This allows
  // objects that must be deleted on the originating thread to be bound into the
  // |task| and |reply| Closures.  In particular, it can be useful to use
  // WeakPtr<> in the |reply| Closure so that the reply operation can be
  // canceled. See the following pseudo-code:
  //
  // class DataBuffer : public RefCountedThreadSafe<DataBuffer> {
  //  public:
  //   // Called to add data into a buffer.
  //   void AddData(void* buf, size_t length);
  //   ...
  // };
  //
  //
  // class DataLoader : public SupportsWeakPtr<DataLoader> {
  //  public:
  //    void GetData() {
  //      scoped_refptr<DataBuffer> buffer = new DataBuffer();
  //      target_thread_.message_loop_proxy()->PostTaskAndReply(
  //          FROM_HERE,
  //          base::Bind(&DataBuffer::AddData, buffer),
  //          base::Bind(&DataLoader::OnDataReceived, AsWeakPtr(), buffer));
  //    }
  //
  //  private:
  //    void OnDataReceived(scoped_refptr<DataBuffer> buffer) {
  //      // Do something with buffer.
  //    }
  // };
  //
  //
  // Things to notice:
  //   * Results of |task| are shared with |reply| by binding a shared argument
  //     (a DataBuffer instance).
  //   * The DataLoader object has no special thread safety.
  //   * The DataLoader object can be deleted while |task| is still running,
  //     and the reply will cancel itself safely because it is bound to a
  //     WeakPtr<>.
  bool PostTaskAndReply(const tracked_objects::Location& from_here,
                        const Closure& task,
                        const Closure& reply);

  template <class T>
  bool DeleteSoon(const tracked_objects::Location& from_here,
                  const T* object) {
    return subtle::DeleteHelperInternal<T, bool>::DeleteOnMessageLoop(
        this, from_here, object);
  }
  template <class T>
  bool ReleaseSoon(const tracked_objects::Location& from_here,
                   T* object) {
    return subtle::ReleaseHelperInternal<T, bool>::ReleaseOnMessageLoop(
        this, from_here, object);
  }

  // Gets the MessageLoopProxy for the current message loop, creating one if
  // needed.
  static scoped_refptr<MessageLoopProxy> current();

 protected:
  friend class RefCountedThreadSafe<MessageLoopProxy, MessageLoopProxyTraits>;
  friend struct MessageLoopProxyTraits;

  MessageLoopProxy();
  virtual ~MessageLoopProxy();

  // Called when the proxy is about to be deleted. Subclasses can override this
  // to provide deletion on specific threads.
  virtual void OnDestruct() const;

 private:
  template <class T, class R> friend class subtle::DeleteHelperInternal;
  template <class T, class R> friend class subtle::ReleaseHelperInternal;
  bool DeleteSoonInternal(const tracked_objects::Location& from_here,
                          void(*deleter)(const void*),
                          const void* object);
  bool ReleaseSoonInternal(const tracked_objects::Location& from_here,
                           void(*releaser)(const void*),
                           const void* object);
};

struct MessageLoopProxyTraits {
  static void Destruct(const MessageLoopProxy* proxy) {
    proxy->OnDestruct();
  }
};

}  // namespace base

#endif  // BASE_MESSAGE_LOOP_PROXY_H_
