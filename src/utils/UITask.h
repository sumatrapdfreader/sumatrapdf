/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

/*
Unique ids for each uitask::Post(), for debugging.
I get crashes when executing tasks and this allows me to at least find out which
closure crashes.
*/

#define TASK_NAMES(V)                 \
    V(TaskUpdateProgress)             \
    V(TaskNotifOnTimerRemove)         \
    V(TaskNotifOnTimerDelete)         \
    V(TaskFindUpdateStatus)           \
    V(TaskFindEnd1)                   \
    V(TaskFindEnd2)                   \
    V(TaskPrintUpdateProgress)        \
    V(PrintDeleteThread)              \
    V(TaskCommandPaletteDelete)       \
    V(TaskChmModelOnDocumentComplete) \
    V(TaskGoToTocTreeItem)            \
    V(TaskNotifWndProcRemove)         \
    V(TaskNotifWndProcDelete)         \
    V(TaskReloadSettings)             \
    V(TaskHideMissingFiles)           \
    V(TaskRepaintAsync)               \
    V(TaskSetThumbnail)               \
    V(TaskScheduleReloadTab)          \
    V(TaskLoadDocumentAsyncFinish)    \
    V(TaskGoToFavorite)               \
    V(TaskGoToFavorite2)              \
    V(TaksClearHistoryAsyncPart)      \
    V(TaskShowAutoUpdateDialog)       \
    V(TaskCheckForUpdateAsync)        \
    V(TaskUndefined)

#define DEF_TASK(id) id,
enum { TASK_NAMES(DEF_TASK) };

#undef DEF_TASK

namespace uitask {

// Call Initialize() at program startup and Destroy() at the end
void Initialize();
void Destroy();

// call only from the same thread as Initialize() and Destroy()
void DrainQueue();

void Post(int taskId, const std::function<void()>&);
// void PostOptimized(const std::function<void()>& f);
} // namespace uitask
