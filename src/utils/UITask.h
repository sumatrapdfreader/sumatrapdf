#ifndef UITask_h
#define UITask_h

// TODO:
// - rename to UITask
// - make independent of Sumatra code (no WindowInfo)
// - replace UiMsg with UITask
// - do I have to add a way to notify HWND via WM_NULL so that it executes
//   tasks as soon as possible? Can be done by having UITask take HWND or
//   maybe PostThreadMessage() to ui thread?

class WindowInfo;

// Base class for code that has to be executed on UI thread. Derive your class
// from UIThreadWorkItem and call QueueWorkItem to schedule execution
// of its Execute() method on UI thread.
class UIThreadWorkItem
{
public:
    WindowInfo *win;

    UIThreadWorkItem(WindowInfo *win) : win(win) {}
    virtual ~UIThreadWorkItem() {}
    virtual void Execute() = 0;
};

void QueueWorkItem(UIThreadWorkItem *wi);
void ExecuteUITasks();

namespace uitask {

// call Initialize() at program startup and Destroy() at the end
void    Initialize();
void    Destroy();

// Called from any thread, posts a message to a queue, to be processed by ui thread
void    Post(UIThreadWorkItem *msg);

// Called on ui thread (e.g. in an event loop) to process queued messages.
// Removes the message from the queue.
// Returns NULL if there are no more messages.
UIThreadWorkItem * RetrieveNext();

// Gets a handle of uimsg queque event. This event gets notified when
// a new item is posted to the queue. Can be used to awake ui event
// loop if MsgWaitForMultipleObjects() is used, but that's not
// necessary.
HANDLE  GetQueueEvent();

}

#endif

