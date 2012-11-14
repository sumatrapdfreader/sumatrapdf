#include "BaseUtil.h"
#include "UITask.h"

#include "WindowInfo.h"

static UIThreadWorkItemQueue  gUIThreadMarshaller;

void QueueWorkItem(UIThreadWorkItem *wi)
{
    gUIThreadMarshaller.Queue(wi);
}

void ExecuteUITasks()
{
    gUIThreadMarshaller.Execute();
}

void UIThreadWorkItemQueue::Execute() {
    // no need to acquire a lock for this check
    if (items.Count() == 0)
        return;

    ScopedCritSec scope(&cs);
    while (items.Count() > 0) {
        UIThreadWorkItem *wi = items.At(0);
        items.RemoveAt(0);
        wi->Execute();
        delete wi;
    }
}

void UIThreadWorkItemQueue::Queue(UIThreadWorkItem *item)
{
    if (!item)
        return;

    ScopedCritSec scope(&cs);
    items.Append(item);
    if (item->win) {
        // hwndCanvas is less likely to enter internal message pump (during which
        // the messages are not visible to our processing in top-level message pump)
        PostMessage(item->win->hwndCanvas, WM_NULL, 0, 0);
    }
}

