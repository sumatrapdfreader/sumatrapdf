/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

/* A very simple thread class that allows stopping a thread */
class ThreadBase {
  private:
    LONG cancelRequested = 0;
    int threadNo = 0;
    HANDLE hThread = nullptr;

    static DWORD WINAPI ThreadProc(void* data);

  protected:
    // for debugging
    AutoFree threadName;

    virtual ~ThreadBase();

    bool WasCancelRequested() {
        LONG res = InterlockedCompareExchange(&cancelRequested, 1, 0);
        return res > 0;
    }

  public:
    // name is for debugging purposes, can be nullptr.
    explicit ThreadBase(const char* name = nullptr);

    // call this to start executing Run() function.
    void Start();

    // request the thread to stop. It's up to Run() function
    // to call WasCancelRequested() and stop processing if it returns true.
    void RequestCancel() {
        InterlockedIncrement(&cancelRequested);
    }

    // synchronously waits for the thread to end
    // returns true if thread stopped by itself and false if waiting timed out
    bool Join(DWORD waitMs = INFINITE);

    // get a unique number that identifies a thread and unlike an
    // address of the object, will not be reused
    LONG GetNo() const {
        return threadNo;
    }

    // over-write this to implement the actual thread functionality
    // note: for longer running threads, make sure to occasionally poll WasCancelRequested
    virtual void Run() = 0;
};

void SetThreadName(DWORD threadId, const char* threadName);

void RunAsync(const std::function<void()>&);
