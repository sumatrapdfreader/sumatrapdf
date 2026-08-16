/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct PlatformCanvas;
struct PasswordUI;
struct Pixmap;

struct DocumentView {
    PlatformCanvas* canvas = nullptr;
    void* data = nullptr;
    Func0 onStateChanged;
    Func1<Str> onOpenUrl;
    Func1<Str> onOpenFile;
    Func1<Str> onCopyText;

    DocumentView() = default;
    DocumentView(const DocumentView&) = delete;
    DocumentView& operator=(const DocumentView&) = delete;
    ~DocumentView();

    static DocumentView* Create();

    bool Open(Str path, PasswordUI* pwdUI = nullptr);
    void* NativeWidget() const;
    void Focus();
    int PageCount() const;
    int CurrentPageNo() const;
    void GoToPage(int pageNo);
    void SetContinuous(bool continuous);
    bool IsContinuous() const;
    void SetZoom(float zoomVirtual, Point* anchor = nullptr);
    float Zoom() const;
    void RotateBy(int degrees);
    int Rotation() const;
    bool CanPrint() const;
    RectF PageMediabox(int pageNo) const;
    float FileDPI() const;
    Pixmap* RenderPageForPrint(int pageNo, float zoom) const;
    bool HasTextSelection() const;
    void SelectAll();
    void CopySelection();
    bool FindText(Str text, bool forward, bool restart);
    int TocItemCount() const;
    Str TocItemTitle(int index) const;
    int TocItemDepth(int index) const;
    bool GoToTocItem(int index);
    int PropertyCount();
    Str PropertyName(int index);
    Str PropertyValue(int index);
};
