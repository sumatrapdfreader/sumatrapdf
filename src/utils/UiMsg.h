/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef UiMsg_h
#define UiMsg_h

/* UiMsg solves half of threading problem: triggering an execution of
a piece of code on UI thread from a background thread.

It occurred to me that e.g. task/thread solution from Chrome is not great
because it tries to marshall a piece of code which leads to awkward code
in languages without syntax support for closures, capturing local variables
and garbage collection i.e. C++.

In Go, threads communicate by sending data (not code) over channels. This is
a simplification of this idea in that there is only one globally visible channel
that any thread can use to post messages to ui thread.

This is similar to UIThreadWorkItem but it only deals with data, not code. It's
more flexible in that we don't fix how the code to act on a message has to be
written. It's more tightly coupled in that there must be a dispatch loop in the
app that knows how to handle every message. That's fine as this code has to be
written anyway and centralization of that logic has its advantages over the
approach of scattering it over multiple classes (less code overall, easier to
debug (only one place to set a breakpoint)).

It's different than our rendering queue in that rendering queue combines managing
the queue itself with doing the rendering work. Rendering could possibly be
simplified by breaking it into smaller pieces and using UiMsg for the part that
notifies about finished rendering.

Note that UiMsg is not defined here. In order for this to be used in different
apps, we don't fix anything about UiMsg. I see it as a pure data structure in form
of a tagged union (like e.g. mui::css::Prop) but it could just as well be a class
ith inheritance model like UIThreadWorkItem.

To make this scheme work an app must #include its definition of what UiMsg is before
#including UiMsg.h. E.g.:

#include "UiMsgSumatra.h" // defines UiMsg
#include "UiMsg.h"
*/

// forward declare so that UiMsg.cpp can compile. It's outside of uimsg
// namespace for shorter name (it's unique enough).
class UiMsg;

namespace uimsg {

// call Initialize() at program startup and Destroy() at the end
void    Initialize();
void    Destroy();

// Called from any thread, posts a message to a queue, to be processed by ui thread
void    Post(UiMsg *msg);

// Called on ui thread (e.g. in an event loop) to process queued messages.
// Removes the message from the queue.
// Returns NULL if there are no more messages.
UiMsg * RetrieveNext();

// Gets a handle of uimsg queque event. This event gets notified when
// a new item is posted to the queue. Can be used to awake ui event
// loop if MsgWaitForMultipleObjects() is used, but that's not
// necessary.
HANDLE  GetQueueEvent();

}

#endif
