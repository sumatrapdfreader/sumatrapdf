/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct TrackbarCtrl;

struct TrackbarPosChangingEvent : WndEvent {
    TrackbarCtrl* trackbarCtrl = nullptr;
    int pos = -1;
    NMTRBTHUMBPOSCHANGING* info = nullptr;
};

using TrackbarPoschangingHandler = std::function<void(TrackbarPosChangingEvent*)>;

struct TrackbarCtrl : WindowBase {
    // set before Create()
    bool isHorizontal = true;
    int rangeMin = 1;
    int rangeMax = 5;

    Size idealSize{};

    // for WM_NOTIFY with TRBN_THUMBPOSCHANGING
    TrackbarPoschangingHandler onPosChanging = nullptr;

    explicit TrackbarCtrl(HWND parent);
    ~TrackbarCtrl() override;

    bool Create() override;

    Size GetIdealSize() override;
    void SetRange(int min, int max);
    void SetValue(int);
    int GetValue();
};
