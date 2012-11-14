/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef UITask_h
#define UITask_h

// TODO:
// - do I have to add a way to notify HWND via WM_NULL so that it executes
//   tasks as soon as possible? Can be done by having UITask take HWND or
//   maybe PostThreadMessage() to ui thread?

// Base class for code that has to be executed on UI thread. Derive your class
// from UITask and call uitask::Post to schedule execution
// of its Execute() method on UI thread.
class UITask
{
public:
    UITask() {}
    virtual ~UITask() {}
    virtual void Execute() = 0;
};

namespace uitask {

// call Initialize() at program startup and Destroy() at the end
void    Initialize();
void    Destroy();

// Called from any thread, posts a message to a queue, to be processed by ui thread
void    Post(UITask *);

// Called on ui thread (e.g. in an event loop) to process queued messages.
// Removes the message from the queue.
// Returns NULL if there are no more messages.
UITask * RetrieveNext();

void ExecuteAll();

// Gets a handle of uimsg queque event. This event gets notified when
// a new item is posted to the queue. Can be used to awake ui event
// loop if MsgWaitForMultipleObjects() is used, but that's not
// necessary.
HANDLE  GetQueueEvent();

}

#endif

