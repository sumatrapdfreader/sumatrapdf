#!/usr/bin/env python3
"""Port wingui char* APIs to Str/WStr."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[2] / "src" / "wingui"

REPLACEMENTS = [
    # Layout.h
    ("void dbglayoutf(const char* fmt, ...);", "void dbglayoutf(Str fmt, ...);"),
    ("void LogConstraints(Constraints c, const char* suffix);", "void LogConstraints(Constraints c, Str suffix);"),
    # Layout.cpp
    ("void dbglayoutf(const char* fmt, ...) {", "void dbglayoutf(Str fmt, ...) {"),
    ("AutoFreeStr s = str::FmtV(fmt, args).s;", "AutoFreeStr s = str::FmtV(fmt.s, args).s;"),
    ("static void LogAppendNum(StrBuilder& s, int n, const char* suffix) {", "static void LogAppendNum(StrBuilder& s, int n, Str suffix) {"),
    ("void LogConstraints(Constraints c, const char* suffix) {", "void LogConstraints(Constraints c, Str suffix) {"),
    # WinGui.h
    ("    const char* text = nullptr;\n    bool isRtl = false;\n};\n\nstruct CreateCustomArgs {", "    Str text;\n    bool isRtl = false;\n};\n\nstruct CreateCustomArgs {"),
    ("    const char* title = nullptr;", "    Str title;"),
    ("    void SetText(const char*);", "    void SetText(Str);"),
    ("        const char* text = nullptr;\n        bool isRtl = false;\n    };\n\n    Static();", "        Str text;\n        bool isRtl = false;\n    };\n\n    Static();"),
    ("        const char* text = nullptr;\n        bool isRtl = false;\n    };\n\n    Func0 onClick{};", "        Str text;\n        bool isRtl = false;\n    };\n\n    Func0 onClick{};"),
    ("Button* CreateButton(HWND parent, const char* s, const Func0& onClick, bool isRtl);", "Button* CreateButton(HWND parent, Str s, const Func0& onClick, bool isRtl);"),
    ("Button* CreateDefaultButton(HWND parent, const char* s, bool isRtl);", "Button* CreateDefaultButton(HWND parent, Str s, bool isRtl);"),
    ("    int Add(const char* s, const Rect& rc, bool multiline);", "    int Add(Str s, const Rect& rc, bool multiline);"),
    ("    void Update(int id, const char* s, const Rect& rc, bool multiline);", "    void Update(int id, Str s, const Rect& rc, bool multiline);"),
    ("    int SetSingle(const char* s, const Rect& rc, bool multiline);", "    int SetSingle(Str s, const Rect& rc, bool multiline);"),
    ("    const char* s;", "    Str s;"),
    ("        const char* cueText = nullptr;\n        const char* text = nullptr;", "        Str cueText;\n        Str text;"),
    ("        const char* text = nullptr;\n        State initialState = State::Unchecked;", "        Str text;\n        State initialState = State::Unchecked;"),
    ("    void SetItemsSeqStrings(const char* items);", "    void SetItemsSeqStrings(SeqStrings items);"),
    ("    void SetCueBanner(const char*);", "    void SetCueBanner(Str);"),
    ("    void SetTextAndTooltip(int idx, const char* text, const char* tooltip);", "    void SetTextAndTooltip(int idx, Str text, Str tooltip);"),
    # Wnd.cpp
    ("void Wnd::SetText(const char* s) {\n    if (!s) {\n        s = \"\";\n    }", "void Wnd::SetText(Str s) {\n    if (!s) {\n        s = Str(\"\");\n    }"),
    # Button.cpp
    ("Button* CreateButton(HWND parent, const char* s, const Func0& onClick, bool isRtl) {", "Button* CreateButton(HWND parent, Str s, const Func0& onClick, bool isRtl) {"),
    ("Button* CreateDefaultButton(HWND parent, const char* s, bool isRtl) {", "Button* CreateDefaultButton(HWND parent, Str s, bool isRtl) {"),
    # Tooltip.cpp
    ("static void SetMaxWidthForText(HWND hwnd, const char* s, bool multiline) {", "static void SetMaxWidthForText(HWND hwnd, Str s, bool multiline) {"),
    ("static bool TooltipUpdateText(HWND hwnd, HWND owner, int id, const char* s, bool multiline) {", "static bool TooltipUpdateText(HWND hwnd, HWND owner, int id, Str s, bool multiline) {"),
    ("int Tooltip::Add(const char* s, const Rect& rc, bool multiline) {", "int Tooltip::Add(Str s, const Rect& rc, bool multiline) {"),
    ("void Tooltip::Update(int id, const char* s, const Rect& rc, bool multiline) {", "void Tooltip::Update(int id, Str s, const Rect& rc, bool multiline) {"),
    ("int Tooltip::SetSingle(const char* s, const Rect& rc, bool multiline) {", "int Tooltip::SetSingle(Str s, const Rect& rc, bool multiline) {"),
    ('        s = (const char*)ShortenStringUtf8InTheMiddleTemp(s, 250);', "        s = Str(ShortenStringUtf8InTheMiddleTemp(s, 250));"),
    # Edit.cpp
    ("static bool EditSetCueText(HWND hwnd, const char* s) {", "static bool EditSetCueText(HWND hwnd, Str s) {"),
    # DropDown.cpp
    ("void DropDown::SetCueBanner(const char* sv) {", "void DropDown::SetCueBanner(Str sv) {"),
    ("static void DropDownItemsFromStringArray(StrVec& items, const char* strings) {", "static void DropDownItemsFromStringArray(StrVec& items, SeqStrings strings) {"),
    ("void DropDown::SetItemsSeqStrings(const char* items) {", "void DropDown::SetItemsSeqStrings(SeqStrings items) {"),
    # TabsCtrl.cpp
    ("void TabsCtrl::SetTextAndTooltip(int idx, const char* text, const char* tooltip) {", "void TabsCtrl::SetTextAndTooltip(int idx, Str text, Str tooltip) {"),
    # WebView.h
    ("using WebViewMsgCb = Func1<const char*>;", "using WebViewMsgCb = Func1<Str>;"),
    ("    bool (*navigationStarting)(void* ctx, const char* url, bool newWindow) = nullptr;", "    bool (*navigationStarting)(void* ctx, Str url, bool newWindow) = nullptr;"),
    ("    void (*navigationCompleted)(void* ctx, const char* url, bool success) = nullptr;", "    void (*navigationCompleted)(void* ctx, Str url, bool success) = nullptr;"),
    ("    bool (*getResource)(void* ctx, const char* path, WebViewResourceResult* res) = nullptr;", "    bool (*getResource)(void* ctx, Str path, WebViewResourceResult* res) = nullptr;"),
    ("    void Eval(const char* js);", "    void Eval(Str js);"),
    ("    void SetHtml(const char* html);", "    void SetHtml(Str html);"),
    ("    void Init(const char* js);", "    void Init(Str js);"),
    ("    void Navigate(const char* url);", "    void Navigate(Str url);"),
    ("    void QueuePendingOp(PendingWebViewOp::Kind kind, const char* text);", "    void QueuePendingOp(PendingWebViewOp::Kind kind, Str text);"),
    ("    virtual void OnBrowserMessage(const char* msg);", "    virtual void OnBrowserMessage(Str msg);"),
    # WebView.cpp
    ("void WebviewWnd::QueuePendingOp(PendingWebViewOp::Kind kind, const char* text) {", "void WebviewWnd::QueuePendingOp(PendingWebViewOp::Kind kind, Str text) {"),
    ("    op.text = str::Dup(text ? text : \"\");", "    op.text = str::Dup(text ? text : Str(\"\"));"),
    ("void WebviewWnd::Eval(const char* js) {", "void WebviewWnd::Eval(Str js) {"),
    ("void WebviewWnd::SetHtml(const char* html) {", "void WebviewWnd::SetHtml(Str html) {"),
    ("void WebviewWnd::Init(const char* js) {", "void WebviewWnd::Init(Str js) {"),
    ("void WebviewWnd::Navigate(const char* url) {", "void WebviewWnd::Navigate(Str url) {"),
    ("static void OnBrowserMessageCbHwnd(void* hwndVoid, const char* msg);", "static void OnBrowserMessageCbHwnd(void* hwndVoid, Str msg);"),
    ("        auto fn = MkFunc1<void, const char*>(OnBrowserMessageCbHwnd, (void*)hwnd);", "        auto fn = MkFunc1<void, Str>(OnBrowserMessageCbHwnd, (void*)hwnd);"),
    ("void WebviewWnd::OnBrowserMessage(const char* msg) {", "void WebviewWnd::OnBrowserMessage(Str msg) {"),
    ("static void OnBrowserMessageCbHwnd(void* hwndVoid, const char* msg) {", "static void OnBrowserMessageCbHwnd(void* hwndVoid, Str msg) {"),
    ("    auto fn = MkFunc1<void, const char*>(OnBrowserMessageCbHwnd, (void*)hwnd);", "    auto fn = MkFunc1<void, Str>(OnBrowserMessageCbHwnd, (void*)hwnd);"),
    ("void WebviewWnd::Eval(const char*) {}", "void WebviewWnd::Eval(Str) {}"),
    ("void WebviewWnd::SetHtml(const char*) {}", "void WebviewWnd::SetHtml(Str) {}"),
    ("void WebviewWnd::Init(const char*) {}", "void WebviewWnd::Init(Str) {}"),
    ("void WebviewWnd::Navigate(const char*) {}", "void WebviewWnd::Navigate(Str) {}"),
    ("void WebviewWnd::QueuePendingOp(PendingWebViewOp::Kind, const char*) {}", "void WebviewWnd::QueuePendingOp(PendingWebViewOp::Kind, Str) {}"),
    ("void WebviewWnd::OnBrowserMessage(const char*) {}", "void WebviewWnd::OnBrowserMessage(Str) {}"),
    ("static TempWStr MimeHeaderFromContentType(const char* contentType) {", "static TempWStr MimeHeaderFromContentType(Str contentType) {"),
    ("                                              size_t dataLen, const char* contentType, int statusCode) {", "                                              size_t dataLen, Str contentType, int statusCode) {"),
    # HtmlWindow.h
    ("bool IsBlankUrl(const char*);", "bool IsBlankUrl(Str);"),
    ("    virtual bool OnBeforeNavigate(const char* url, bool newWindow) = 0;", "    virtual bool OnBeforeNavigate(Str url, bool newWindow) = 0;"),
    ("    virtual void OnDocumentComplete(const char* url) = 0;", "    virtual void OnDocumentComplete(Str url) = 0;"),
    ("    virtual ByteSlice GetDataForUrl(const char* url) = 0;", "    virtual ByteSlice GetDataForUrl(Str url) = 0;"),
    ("    virtual void DownloadData(const char* url, const ByteSlice& data) = 0;", "    virtual void DownloadData(Str url, const ByteSlice& data) = 0;"),
    ("    void NavigateToUrl(const char* url);", "    void NavigateToUrl(Str url);"),
    ("    void NavigateToDataUrl(const char* url);", "    void NavigateToDataUrl(Str url);"),
    ("    void SetHtml(const ByteSlice&, const char* url = nullptr);", "    void SetHtml(const ByteSlice&, Str url = nullptr);"),
    # HtmlWindow.cpp
    ("bool IsBlankUrl(const char* url) {", "bool IsBlankUrl(Str url) {"),
    ("void HtmlWindow::NavigateToDataUrl(const char* url) {", "void HtmlWindow::NavigateToDataUrl(Str url) {"),
    ("void HtmlWindow::NavigateToUrl(const char* urlA) {", "void HtmlWindow::NavigateToUrl(Str urlA) {"),
    ("void HtmlWindow::SetHtml(const ByteSlice& d, const char* url) {", "void HtmlWindow::SetHtml(const ByteSlice& d, Str url) {"),
    # ChmDocView.h
    ("    void NavigateToDataUrl(const char* url);", "    void NavigateToDataUrl(Str url);"),
    ("    static bool ResourceGet(void* ctx, const char* path, WebViewResourceResult* res);", "    static bool ResourceGet(void* ctx, Str path, WebViewResourceResult* res);"),
    ("    static bool NavigationStarting(void* ctx, const char* url, bool newWindow);", "    static bool NavigationStarting(void* ctx, Str url, bool newWindow);"),
    ("    static void NavigationCompleted(void* ctx, const char* url, bool success);", "    static void NavigationCompleted(void* ctx, Str url, bool success);"),
    # ChmDocView.cpp
    ("    void OnBrowserMessage(const char* msg) override;", "    void OnBrowserMessage(Str msg) override;"),
    ("void ChmWebviewWnd::OnBrowserMessage(const char* msg) {", "void ChmWebviewWnd::OnBrowserMessage(Str msg) {"),
    ("bool ChmDocView::ResourceGet(void* ctx, const char* path, WebViewResourceResult* res) {", "bool ChmDocView::ResourceGet(void* ctx, Str path, WebViewResourceResult* res) {"),
    ("bool ChmDocView::NavigationStarting(void* ctx, const char* url, bool newWindow) {", "bool ChmDocView::NavigationStarting(void* ctx, Str url, bool newWindow) {"),
    ("void ChmDocView::NavigationCompleted(void* ctx, const char* url, bool success) {", "void ChmDocView::NavigationCompleted(void* ctx, Str url, bool success) {"),
    ("void ChmDocView::NavigateToDataUrl(const char* url) {", "void ChmDocView::NavigateToDataUrl(Str url) {"),
]

CALLER_REPLACEMENTS = [
    # ChmModel.cpp HtmlWindowHandler
    ("    bool OnBeforeNavigate(const char* url, bool newWindow) override { return cm->OnBeforeNavigate(url, newWindow); }", "    bool OnBeforeNavigate(Str url, bool newWindow) override { return cm->OnBeforeNavigate(url, newWindow); }"),
    ("    void OnDocumentComplete(const char* url) override { cm->OnDocumentComplete(url); }", "    void OnDocumentComplete(Str url) override { cm->OnDocumentComplete(url); }"),
    ("    ByteSlice GetDataForUrl(const char* url) override { return cm->GetDataForUrl(url); }", "    ByteSlice GetDataForUrl(Str url) override { return cm->GetDataForUrl(url); }"),
    ("    void DownloadData(const char* url, const ByteSlice& data) override { cm->DownloadData(url, data); }", "    void DownloadData(Str url, const ByteSlice& data) override { cm->DownloadData(url, data); }"),
    ("    bool OnBeforeNavigate(const char*, bool newWindow) override;", "    bool OnBeforeNavigate(Str, bool newWindow) override;"),
    ("    void OnDocumentComplete(const char* url) override;", "    void OnDocumentComplete(Str url) override;"),
    ("    ByteSlice GetDataForUrl(const char* url) override;", "    ByteSlice GetDataForUrl(Str url) override;"),
    ("    void DownloadData(const char*, const ByteSlice&) override;", "    void DownloadData(Str, const ByteSlice&) override;"),
    ("bool ChmThumbnailTask::OnBeforeNavigate(const char*, bool newWindow) {", "bool ChmThumbnailTask::OnBeforeNavigate(Str, bool newWindow) {"),
    ("ByteSlice ChmThumbnailTask::GetDataForUrl(const char* url) {", "ByteSlice ChmThumbnailTask::GetDataForUrl(Str url) {"),
    ("    char* plainUrl = url::GetFullPathTemp(url);", "    TempStr plainUrl = url::GetFullPathTemp(url);"),
    ("void ChmThumbnailTask::OnDocumentComplete(const char* url) {", "void ChmThumbnailTask::OnDocumentComplete(Str url) {"),
    ("    if (url && *url == '/') {\n        url++;", "    if (url && url.s[0] == '/') {\n        url = Str(url.s + 1, url.len - 1);"),
    ("    logf(\"ChmThumbnailTask::OnDocumentComplete: '%s'\\n\", url);", "    logf(\"ChmThumbnailTask::OnDocumentComplete: '%s'\\n\", url.s);"),
    ("void ChmThumbnailTask::DownloadData(const char*, const ByteSlice&) {}", "void ChmThumbnailTask::DownloadData(Str, const ByteSlice&) {}"),
    # SimpleBrowserWindow.cpp
    ("static bool NavigationStarting(void* ctx, const char* url, bool newWindow) {", "static bool NavigationStarting(void* ctx, Str url, bool newWindow) {"),
    ("static void NavigationCompleted(void* ctx, const char* url, bool success) {", "static void NavigationCompleted(void* ctx, Str url, bool success) {"),
    # AIChatCommon
    ("bool AIChatGetMarkedJsResource(void* ctx, const char* path, WebViewResourceResult* res);", "bool AIChatGetMarkedJsResource(void* ctx, Str path, WebViewResourceResult* res);"),
    ("bool AIChatGetMarkedJsResource(void* ctx, const char* path, WebViewResourceResult* res) {", "bool AIChatGetMarkedJsResource(void* ctx, Str path, WebViewResourceResult* res) {"),
    # SumatraPDF.cpp
    ("static bool ManualGetResource(void* ctx, const char* path, WebViewResourceResult* res) {", "static bool ManualGetResource(void* ctx, Str path, WebViewResourceResult* res) {"),
]

CALLER_FILES = [
    Path(__file__).resolve().parents[2] / "src" / "ChmModel.cpp",
    Path(__file__).resolve().parents[2] / "src" / "SimpleBrowserWindow.cpp",
    Path(__file__).resolve().parents[2] / "src" / "AIChatCommon.h",
    Path(__file__).resolve().parents[2] / "src" / "AIChatCommon.cpp",
    Path(__file__).resolve().parents[2] / "src" / "SumatraPDF.cpp",
]


def apply_replacements(path: Path, replacements):
    text = path.read_text(encoding="utf-8")
    orig = text
    for old, new in replacements:
        if old not in text:
            continue
        text = text.replace(old, new)
    if text != orig:
        path.write_text(text, encoding="utf-8", newline="\n")
        print(f"updated {path.name}")
        return True
    return False


def main():
    for path in sorted(ROOT.glob("*")):
        if path.suffix not in {".h", ".cpp"}:
            continue
        apply_replacements(path, REPLACEMENTS)
    for path in CALLER_FILES:
        if path.exists():
            apply_replacements(path, CALLER_REPLACEMENTS)


if __name__ == "__main__":
    main()