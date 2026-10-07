struct ProgressUpdateData {
    int current = 0;
    int total = 0;
    bool* wasCancelled = nullptr;
};

using ProgressUpdateCb = Func1<ProgressUpdateData*>;

inline void UpdateProgress(const ProgressUpdateCb& cb, int current, int total) {
    ProgressUpdateData data{.current = current, .total = total, .wasCancelled = nullptr};
    cb.Call(&data);
}

inline bool WasCanceled(const ProgressUpdateCb& cb) {
    bool wasCancelled = false;
    ProgressUpdateData data{.current = 0, .total = 0, .wasCancelled = &wasCancelled};
    cb.Call(&data);
    return wasCancelled;
}
