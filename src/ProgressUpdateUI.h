struct ProgressUpdateData {
    int current = 0;
    int total = 0;
    bool* wasCancelled = nullptr;
};

using ProgressUpdateCb = Func1<ProgressUpdateData*>;

// void UpdateProgress(ProgressUpdateCb* cb, int current, int total);
// bool WasCanceled(ProgressUpdateCb* cb);

inline void UpdateProgress(ProgressUpdateCb* cb, int current, int total) {
    if (!cb) {
        return;
    }
    ProgressUpdateData data{current, total, nullptr};
    cb->Call(&data);
}

inline bool WasCanceled(ProgressUpdateCb* cb) {
    if (!cb) {
        return false;
    }
    bool wasCancelled = false;
    ProgressUpdateData data{0, 0, &wasCancelled};
    cb->Call(&data);
    return wasCancelled;
}
