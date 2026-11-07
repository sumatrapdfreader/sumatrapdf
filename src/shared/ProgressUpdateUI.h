struct ProgressUpdateData {
    int current = 0;
    int total = 0;
    bool* wasCancelled = nullptr;
};

using ProgressUpdateCb = Func1<ProgressUpdateData*>;

inline void UpdateProgress(const ProgressUpdateCb& cb, int current, int total) {
    ProgressUpdateData data{.current = current, .total = total};
    cb.Call(&data);
}

inline bool WasCanceled(const ProgressUpdateCb& cb) {
    bool wasCancelled = false;
    ProgressUpdateData data{.wasCancelled = &wasCancelled};
    cb.Call(&data);
    return wasCancelled;
}

// ProgressUpdateCb target for a thread-data struct with WasCanceled() and UpdateProgress()
template <typename T>
void ForwardProgress(T* ftd, ProgressUpdateData* data) {
    if (data->wasCancelled) {
        *data->wasCancelled = ftd->WasCanceled();
        return;
    }
    ftd->UpdateProgress(data->current, data->total);
}
