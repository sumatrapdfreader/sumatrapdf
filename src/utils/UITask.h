/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef UITask_h
#define UITask_h

// TODO:
// - to better encapsulate this functionality, create a hidden window
//   that we'll use to execute tasks, to replace UITask::hwnd. Not sure
//   if it'll work. I tried just PostThreadMessage(WM_NULL, ...) to ui
//   thread but that doesn't get to our processing loop if nested message
//   loop is executed

// Base class for code that has to be executed on UI thread. Derive your class
// from UITask and call uitask::Post to schedule execution
// of its Execute() method on UI thread
// The program also has to periodically call uitask::ExecuteAll()
class UITask
{
public:
    // some tasks need to be processed when doing nested message processing
    // loop. In those cases the OS runs the message loop itself and we don't
    // see those messages in our main loop until nested loop finishes
    // this helps with that problem in that the task can be associated with
    // a hwnd to which we'll send WM_NULL message. WndProc of those windows
    // has to call uitask::ExecuteAll()
    HWND hwnd;

    // for debugging
    const char *name;

    UITask(HWND hwnd=NULL) : hwnd(hwnd), name("UITask") {}
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

