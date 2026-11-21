/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#if defined(SUMATRA_NG)
#include "gui/GpuiBridge.h"
#else
#include "base/Base.h"
#endif
#include "base/File.h"
#include "base/GuessFileType.h"
#include "base/Pixmap.h"
#include "base/FileWatcher.h"
#include "base/UITask.h"
#if OS_WIN
#include "base/Win.h"
#else
#include "base/Launch.h"
#endif
#include "gui/UIModels.h"

#include "Settings.h"
#include "AppTools.h"
#include "AppSettings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "EngineAll.h"
#include "DisplayModel.h"
#include "ImageReader.h"
#include "RenderCache.h"
#include "Notifications.h"
#include "SumatraConfig.h"
#include "Translations.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "AIChatTypes.h"
#include "WindowTab.h"
#include "Toolbar.h"
#include "ImageReplace.h"

#include "SumatraLog.h"

bool ReplaceImageWithData(MainWindow* win, int pageNo, int imageIdx, Str data) {
    DisplayModel* dm = win->AsFixed();
    if (!dm || len(data) == 0) {
        return false;
    }
    if (gRenderCache) {
        gRenderCache->AbortRendering(dm);
    }
    bool ok = EngineMupdfReplaceImage(dm->GetEngine(), pageNo, imageIdx, data);
    ToolbarUpdateStateForWindow(win, true);
    if (!ok) {
        ShowWarningNotification(win, Tr("Couldn't replace the image"), kNotif5SecsTimeOut);
        return false;
    }
    MainWindowRerender(win);
    return true;
}

bool ImageElementPos(IPageElement* el, int* pageNo, int* imageIdx) {
    if (!el || !el->Is(kindPageElementImage)) {
        return false;
    }
    *pageNo = el->GetPageNo();
    *imageIdx = ((PageElementImage*)el)->imageID;
    return *pageNo > 0 && *imageIdx >= 0;
}

// Replace Image...: the picked file is drawn in place of the image. The picker
// may have run a message loop, which can reload or close the document of tab.
void ReplaceImageFromPath(MainWindow* win, WindowTab* tab, int pageNo, int imageIdx, Str path) {
    if (len(path) == 0 || !IsMainWindowValidAndNotClosing(win) || win->CurrentTab() != tab) {
        return;
    }
    Str data = file::ReadFile(path);
    if (ReplaceImageWithData(win, pageNo, imageIdx, data)) {
        ShowTemporaryNotification(win, Tr("Image replaced. Save the PDF to keep it."), kNotif5SecsTimeOut);
    }
    str::Free(data);
}

// An image opened in another app by Edit Image in Another App. Every time the
// app saves the file, the PDF's image is replaced with it.
struct ImageEditWatch {
    WindowTab* tab = nullptr;
    EngineBase* engine = nullptr;
    int pageNo = 0;
    int imageIdx = 0;
    Str path;
    WatchedFile* wf = nullptr;
};

// never freed: a change can be queued for one whose document is already gone
static Vec<ImageEditWatch*> gImageEditWatches;
static int gImageEditCounter = 0;

// the window showing the document w was made for, or null if it's closed
static MainWindow* ImageEditWatchWindow(ImageEditWatch* w) {
    for (MainWindow* win : gWindows) {
        for (WindowTab* tab : win->Tabs()) {
            if (tab == w->tab && tab->AsFixed() && tab->AsFixed()->GetEngine() == w->engine) {
                return win;
            }
        }
    }
    return nullptr;
}

static bool ImageDataIsValid(Str data) {
    Pixmap* px = len(data) > 0 ? PixmapFromData(data) : nullptr;
    FreePixmap(px);
    return px != nullptr;
}

static void ApplyImageEdit(ImageEditWatch* w) {
    if (!w->wf) {
        return;
    }
    MainWindow* win = ImageEditWatchWindow(w);
    if (!win) {
        FileWatcherUnsubscribe(w->wf);
        w->wf = nullptr;
        return;
    }
    // only the current tab is re-rendered; edits to a background tab's image wait
    if (win->CurrentTab() != w->tab) {
        return;
    }
    Str data = file::ReadFile(w->path);
    // the editor can still be writing it; the next change notification comes
    if (ImageDataIsValid(data) && ReplaceImageWithData(win, w->pageNo, w->imageIdx, data)) {
        ShowTemporaryNotification(win, Tr("Image updated. Save the PDF to keep it."), kNotif5SecsTimeOut);
    }
    str::Free(data);
}

static void OnImageEditFileChanged(ImageEditWatch* w) {
    // called on the file watcher's thread
    uitask::Post(MkFunc0(ApplyImageEdit, w), nullptr);
}

static ImageEditWatch* StartImageEditWatch(MainWindow* win, int pageNo, int imageIdx) {
    WindowTab* tab = win->CurrentTab();
    EngineBase* engine = win->AsFixed()->GetEngine();
    for (ImageEditWatch* w : gImageEditWatches) {
        if (w->wf && w->tab == tab && w->engine == engine && w->pageNo == pageNo && w->imageIdx == imageIdx) {
            return w;
        }
    }
    Str png = EngineMupdfImageAsPng(engine, pageNo, imageIdx);
    if (len(png) == 0) {
        return nullptr;
    }
    TempStr name = fmt("SumatraPDF-image-%d-%d.png", CurrentProcessId(), ++gImageEditCounter);
    TempStr path = path::JoinTemp(GetTempDirPathTemp(), name);
    bool written = file::WriteFile(path, png);
    str::Free(png);
    if (!written) {
        return nullptr;
    }
    auto* w = new ImageEditWatch();
    w->tab = tab;
    w->engine = engine;
    w->pageNo = pageNo;
    w->imageIdx = imageIdx;
    w->path = str::Dup(path);
    w->wf = FileWatcherSubscribe(w->path, MkFunc0(OnImageEditFileChanged, w));
    VecAppend(gImageEditWatches, w);
    return w;
}

// Edit Image in Another App: saves the image to a temporary PNG, opens it in
// the app the system edits PNGs with (Paint on Windows unless changed, else
// the desktop's default PNG app) and replaces the image whenever it saves it.
void EditImageExternally(MainWindow* win, IPageElement* imageEl) {
    int pageNo = 0;
    int imageIdx = 0;
    if (!ImageElementPos(imageEl, &pageNo, &imageIdx) || !win->AsFixed() || !CanAccessDisk()) {
        return;
    }
    ImageEditWatch* w = StartImageEditWatch(win, pageNo, imageIdx);
    if (!w) {
        ShowWarningNotification(win, Tr("Couldn't save the image for editing"), kNotif5SecsTimeOut);
        return;
    }
    if (gForTesting) {
        return;
    }
#if OS_WIN
    if (!LaunchFileShell(w->path, {}, StrL("edit"))) {
        LaunchFileShell(StrL("mspaint.exe"), fmt("\"%s\"", w->path));
    }
#else
    LaunchFileShell(w->path);
#endif
}

// replace <path> | edit: "path=<the temporary file>"
TempStr TestImageReplace(MainWindow* win, Str action, int pageNo, int imageIdx, Str path) {
    if (!win || !win->AsFixed()) {
        return StrL("error=no-document");
    }
    if (str::Eq(action, StrL("replace"))) {
        Str data = len(path) > 0 ? file::ReadFile(path) : Str{};
        bool ok = ReplaceImageWithData(win, pageNo, imageIdx, data);
        str::Free(data);
        return fmt("ok=%d dirty=%d", (int)ok, (int)EngineHasUnsavedAnnotations(win->AsFixed()->GetEngine()));
    }
    if (str::Eq(action, StrL("edit"))) {
        ImageEditWatch* w = StartImageEditWatch(win, pageNo, imageIdx);
        return w ? fmt("path=%s", w->path) : TempStr(StrL("error=no-image"));
    }
    return fmt("dirty=%d", (int)EngineHasUnsavedAnnotations(win->AsFixed()->GetEngine()));
}
