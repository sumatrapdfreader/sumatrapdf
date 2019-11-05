
class ProgressUpdateUI {
  public:
    virtual void UpdateProgress(int current, int total) = 0;
    virtual bool WasCanceled() = 0;
    virtual ~ProgressUpdateUI() {
    }
};
