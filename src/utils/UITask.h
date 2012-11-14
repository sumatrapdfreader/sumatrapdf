#ifndef UITask_h
#define UITask_h

// TODO:
// - rename to UITask
// - make independent of Sumatra code (no WindowInfo)
// - replace UiMsg with UITask

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

class UIThreadWorkItemQueue
{
    CRITICAL_SECTION        cs;
    Vec<UIThreadWorkItem *> items;

public:
    UIThreadWorkItemQueue() {
        InitializeCriticalSection(&cs);
    }

    ~UIThreadWorkItemQueue() {
        DeleteCriticalSection(&cs);
        DeleteVecMembers(items);
    }

    void Queue(UIThreadWorkItem *item);

    void Execute();
};

#endif

