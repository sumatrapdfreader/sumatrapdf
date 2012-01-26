// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process_util.h"

#include <fcntl.h>
#include <io.h>
#include <windows.h>
#include <userenv.h>
#include <psapi.h>

#include <ios>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/debug/stack_trace.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/sys_info.h"
#include "base/win/object_watcher.h"
#include "base/win/scoped_handle.h"
#include "base/win/windows_version.h"

// userenv.dll is required for CreateEnvironmentBlock().
#pragma comment(lib, "userenv.lib")

namespace base {

namespace {

// System pagesize. This value remains constant on x86/64 architectures.
const int PAGESIZE_KB = 4;

// Exit codes with special meanings on Windows.
const DWORD kNormalTerminationExitCode = 0;
const DWORD kDebuggerInactiveExitCode = 0xC0000354;
const DWORD kKeyboardInterruptExitCode = 0xC000013A;
const DWORD kDebuggerTerminatedExitCode = 0x40010004;

// Maximum amount of time (in milliseconds) to wait for the process to exit.
static const int kWaitInterval = 2000;

// This exit code is used by the Windows task manager when it kills a
// process.  It's value is obviously not that unique, and it's
// surprising to me that the task manager uses this value, but it
// seems to be common practice on Windows to test for it as an
// indication that the task manager has killed something if the
// process goes away.
const DWORD kProcessKilledExitCode = 1;

// HeapSetInformation function pointer.
typedef BOOL (WINAPI* HeapSetFn)(HANDLE, HEAP_INFORMATION_CLASS, PVOID, SIZE_T);

// Previous unhandled filter. Will be called if not NULL when we intercept an
// exception. Only used in unit tests.
LPTOP_LEVEL_EXCEPTION_FILTER g_previous_filter = NULL;

// Prints the exception call stack.
// This is the unit tests exception filter.
long WINAPI StackDumpExceptionFilter(EXCEPTION_POINTERS* info) {
  debug::StackTrace(info).PrintBacktrace();
  if (g_previous_filter)
    return g_previous_filter(info);
  return EXCEPTION_CONTINUE_SEARCH;
}

// Connects back to a console if available.
void AttachToConsole() {
  if (!AttachConsole(ATTACH_PARENT_PROCESS)) {
    unsigned int result = GetLastError();
    // Was probably already attached.
    if (result == ERROR_ACCESS_DENIED)
      return;

    if (result == ERROR_INVALID_HANDLE || result == ERROR_INVALID_HANDLE) {
      // TODO(maruel): Walk up the process chain if deemed necessary.
    }
    // Continue even if the function call fails.
    AllocConsole();
  }
  // http://support.microsoft.com/kb/105305
  int raw_out = _open_osfhandle(
      reinterpret_cast<intptr_t>(GetStdHandle(STD_OUTPUT_HANDLE)), _O_TEXT);
  *stdout = *_fdopen(raw_out, "w");
  setvbuf(stdout, NULL, _IONBF, 0);

  int raw_err = _open_osfhandle(
      reinterpret_cast<intptr_t>(GetStdHandle(STD_ERROR_HANDLE)), _O_TEXT);
  *stderr = *_fdopen(raw_err, "w");
  setvbuf(stderr, NULL, _IONBF, 0);

  int raw_in = _open_osfhandle(
      reinterpret_cast<intptr_t>(GetStdHandle(STD_INPUT_HANDLE)), _O_TEXT);
  *stdin = *_fdopen(raw_in, "r");
  setvbuf(stdin, NULL, _IONBF, 0);
  // Fix all cout, wcout, cin, wcin, cerr, wcerr, clog and wclog.
  std::ios::sync_with_stdio();
}

void OnNoMemory() {
  // Kill the process. This is important for security, since WebKit doesn't
  // NULL-check many memory allocations. If a malloc fails, returns NULL, and
  // the buffer is then used, it provides a handy mapping of memory starting at
  // address 0 for an attacker to utilize.
  __debugbreak();
  _exit(1);
}

class TimerExpiredTask : public win::ObjectWatcher::Delegate {
 public:
  explicit TimerExpiredTask(ProcessHandle process);
  ~TimerExpiredTask();

  void TimedOut();

  // MessageLoop::Watcher -----------------------------------------------------
  virtual void OnObjectSignaled(HANDLE object);

 private:
  void KillProcess();

  // The process that we are watching.
  ProcessHandle process_;

  win::ObjectWatcher watcher_;

  DISALLOW_COPY_AND_ASSIGN(TimerExpiredTask);
};

TimerExpiredTask::TimerExpiredTask(ProcessHandle process) : process_(process) {
  watcher_.StartWatching(process_, this);
}

TimerExpiredTask::~TimerExpiredTask() {
  TimedOut();
  DCHECK(!process_) << "Make sure to close the handle.";
}

void TimerExpiredTask::TimedOut() {
  if (process_)
    KillProcess();
}

void TimerExpiredTask::OnObjectSignaled(HANDLE object) {
  CloseHandle(process_);
  process_ = NULL;
}

void TimerExpiredTask::KillProcess() {
  // Stop watching the process handle since we're killing it.
  watcher_.StopWatching();

  // OK, time to get frisky.  We don't actually care when the process
  // terminates.  We just care that it eventually terminates, and that's what
  // TerminateProcess should do for us. Don't check for the result code since
  // it fails quite often. This should be investigated eventually.
  base::KillProcess(process_, kProcessKilledExitCode, false);

  // Now, just cleanup as if the process exited normally.
  OnObjectSignaled(process_);
}

}  // namespace

ProcessId GetCurrentProcId() {
  return ::GetCurrentProcessId();
}

ProcessHandle GetCurrentProcessHandle() {
  return ::GetCurrentProcess();
}

bool OpenProcessHandle(ProcessId pid, ProcessHandle* handle) {
  // We try to limit privileges granted to the handle. If you need this
  // for test code, consider using OpenPrivilegedProcessHandle instead of
  // adding more privileges here.
  ProcessHandle result = OpenProcess(PROCESS_DUP_HANDLE | PROCESS_TERMINATE,
                                     FALSE, pid);

  if (result == INVALID_HANDLE_VALUE)
    return false;

  *handle = result;
  return true;
}

bool OpenPrivilegedProcessHandle(ProcessId pid, ProcessHandle* handle) {
  ProcessHandle result = OpenProcess(PROCESS_DUP_HANDLE |
                                     PROCESS_TERMINATE |
                                     PROCESS_QUERY_INFORMATION |
                                     PROCESS_VM_READ |
                                     SYNCHRONIZE,
                                     FALSE, pid);

  if (result == INVALID_HANDLE_VALUE)
    return false;

  *handle = result;
  return true;
}

bool OpenProcessHandleWithAccess(ProcessId pid,
                                 uint32 access_flags,
                                 ProcessHandle* handle) {
  ProcessHandle result = OpenProcess(access_flags, FALSE, pid);

  if (result == INVALID_HANDLE_VALUE)
    return false;

  *handle = result;
  return true;
}

void CloseProcessHandle(ProcessHandle process) {
  CloseHandle(process);
}

ProcessId GetProcId(ProcessHandle process) {
  // Get a handle to |process| that has PROCESS_QUERY_INFORMATION rights.
  HANDLE current_process = GetCurrentProcess();
  HANDLE process_with_query_rights;
  if (DuplicateHandle(current_process, process, current_process,
                      &process_with_query_rights, PROCESS_QUERY_INFORMATION,
                      false, 0)) {
    DWORD id = GetProcessId(process_with_query_rights);
    CloseHandle(process_with_query_rights);
    return id;
  }

  // We're screwed.
  NOTREACHED();
  return 0;
}

bool GetProcessIntegrityLevel(ProcessHandle process, IntegrityLevel *level) {
  if (!level)
    return false;

  if (win::GetVersion() < base::win::VERSION_VISTA)
    return false;

  HANDLE process_token;
  if (!OpenProcessToken(process, TOKEN_QUERY | TOKEN_QUERY_SOURCE,
      &process_token))
    return false;

  win::ScopedHandle scoped_process_token(process_token);

  DWORD token_info_length = 0;
  if (GetTokenInformation(process_token, TokenIntegrityLevel, NULL, 0,
                          &token_info_length) ||
      GetLastError() != ERROR_INSUFFICIENT_BUFFER)
    return false;

  scoped_array<char> token_label_bytes(new char[token_info_length]);
  if (!token_label_bytes.get())
    return false;

  TOKEN_MANDATORY_LABEL* token_label =
      reinterpret_cast<TOKEN_MANDATORY_LABEL*>(token_label_bytes.get());
  if (!token_label)
    return false;

  if (!GetTokenInformation(process_token, TokenIntegrityLevel, token_label,
                           token_info_length, &token_info_length))
    return false;

  DWORD integrity_level = *GetSidSubAuthority(token_label->Label.Sid,
      (DWORD)(UCHAR)(*GetSidSubAuthorityCount(token_label->Label.Sid)-1));

  if (integrity_level < SECURITY_MANDATORY_MEDIUM_RID) {
    *level = LOW_INTEGRITY;
  } else if (integrity_level >= SECURITY_MANDATORY_MEDIUM_RID &&
      integrity_level < SECURITY_MANDATORY_HIGH_RID) {
    *level = MEDIUM_INTEGRITY;
  } else if (integrity_level >= SECURITY_MANDATORY_HIGH_RID) {
    *level = HIGH_INTEGRITY;
  } else {
    NOTREACHED();
    return false;
  }

  return true;
}

bool LaunchProcess(const string16& cmdline,
                   const LaunchOptions& options,
                   ProcessHandle* process_handle) {
  STARTUPINFO startup_info = {};
  startup_info.cb = sizeof(startup_info);
  if (options.empty_desktop_name)
    startup_info.lpDesktop = L"";
  startup_info.dwFlags = STARTF_USESHOWWINDOW;
  startup_info.wShowWindow = options.start_hidden ? SW_HIDE : SW_SHOW;
  PROCESS_INFORMATION process_info;

  DWORD flags = 0;

  if (options.job_handle) {
    flags |= CREATE_SUSPENDED;

    // If this code is run under a debugger, the launched process is
    // automatically associated with a job object created by the debugger.
    // The CREATE_BREAKAWAY_FROM_JOB flag is used to prevent this.
    flags |= CREATE_BREAKAWAY_FROM_JOB;
  }

  if (options.as_user) {
    flags |= CREATE_UNICODE_ENVIRONMENT;
    void* enviroment_block = NULL;

    if (!CreateEnvironmentBlock(&enviroment_block, options.as_user, FALSE))
      return false;

    BOOL launched =
        CreateProcessAsUser(options.as_user, NULL,
                            const_cast<wchar_t*>(cmdline.c_str()),
                            NULL, NULL, options.inherit_handles, flags,
                            enviroment_block, NULL, &startup_info,
                            &process_info);
    DestroyEnvironmentBlock(enviroment_block);
    if (!launched)
      return false;
  } else {
    if (!CreateProcess(NULL,
                       const_cast<wchar_t*>(cmdline.c_str()), NULL, NULL,
                       options.inherit_handles, flags, NULL, NULL,
                       &startup_info, &process_info)) {
      return false;
    }
  }

  if (options.job_handle) {
    if (0 == AssignProcessToJobObject(options.job_handle,
                                      process_info.hProcess)) {
      DLOG(ERROR) << "Could not AssignProcessToObject.";
      KillProcess(process_info.hProcess, kProcessKilledExitCode, true);
      return false;
    }

    ResumeThread(process_info.hThread);
  }

  // Handles must be closed or they will leak.
  CloseHandle(process_info.hThread);

  if (options.wait)
    WaitForSingleObject(process_info.hProcess, INFINITE);

  // If the caller wants the process handle, we won't close it.
  if (process_handle) {
    *process_handle = process_info.hProcess;
  } else {
    CloseHandle(process_info.hProcess);
  }
  return true;
}

bool LaunchProcess(const CommandLine& cmdline,
                   const LaunchOptions& options,
                   ProcessHandle* process_handle) {
  return LaunchProcess(cmdline.GetCommandLineString(), options, process_handle);
}

bool SetJobObjectAsKillOnJobClose(HANDLE job_object) {
  JOBOBJECT_EXTENDED_LIMIT_INFORMATION limit_info = {0};
  limit_info.BasicLimitInformation.LimitFlags =
      JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
  return 0 != SetInformationJobObject(
      job_object,
      JobObjectExtendedLimitInformation,
      &limit_info,
      sizeof(limit_info));
}

// Attempts to kill the process identified by the given process
// entry structure, giving it the specified exit code.
// Returns true if this is successful, false otherwise.
bool KillProcessById(ProcessId process_id, int exit_code, bool wait) {
  HANDLE process = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE,
                               FALSE,  // Don't inherit handle
                               process_id);
  if (!process) {
    DLOG(ERROR) << "Unable to open process " << process_id << " : "
                << GetLastError();
    return false;
  }
  bool ret = KillProcess(process, exit_code, wait);
  CloseHandle(process);
  return ret;
}

bool GetAppOutput(const CommandLine& cl, std::string* output) {
  HANDLE out_read = NULL;
  HANDLE out_write = NULL;

  SECURITY_ATTRIBUTES sa_attr;
  // Set the bInheritHandle flag so pipe handles are inherited.
  sa_attr.nLength = sizeof(SECURITY_ATTRIBUTES);
  sa_attr.bInheritHandle = TRUE;
  sa_attr.lpSecurityDescriptor = NULL;

  // Create the pipe for the child process's STDOUT.
  if (!CreatePipe(&out_read, &out_write, &sa_attr, 0)) {
    NOTREACHED() << "Failed to create pipe";
    return false;
  }

  // Ensure we don't leak the handles.
  win::ScopedHandle scoped_out_read(out_read);
  win::ScopedHandle scoped_out_write(out_write);

  // Ensure the read handle to the pipe for STDOUT is not inherited.
  if (!SetHandleInformation(out_read, HANDLE_FLAG_INHERIT, 0)) {
    NOTREACHED() << "Failed to disabled pipe inheritance";
    return false;
  }

  std::wstring writable_command_line_string(cl.GetCommandLineString());

  PROCESS_INFORMATION proc_info = { 0 };
  STARTUPINFO start_info = { 0 };

  start_info.cb = sizeof(STARTUPINFO);
  start_info.hStdOutput = out_write;
  // Keep the normal stdin and stderr.
  start_info.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
  start_info.hStdError = GetStdHandle(STD_ERROR_HANDLE);
  start_info.dwFlags |= STARTF_USESTDHANDLES;

  // Create the child process.
  if (!CreateProcess(NULL,
                     &writable_command_line_string[0],
                     NULL, NULL,
                     TRUE,  // Handles are inherited.
                     0, NULL, NULL, &start_info, &proc_info)) {
    NOTREACHED() << "Failed to start process";
    return false;
  }

  // We don't need the thread handle, close it now.
  CloseHandle(proc_info.hThread);

  // Close our writing end of pipe now. Otherwise later read would not be able
  // to detect end of child's output.
  scoped_out_write.Close();

  // Read output from the child process's pipe for STDOUT
  const int kBufferSize = 1024;
  char buffer[kBufferSize];

  for (;;) {
    DWORD bytes_read = 0;
    BOOL success = ReadFile(out_read, buffer, kBufferSize, &bytes_read, NULL);
    if (!success || bytes_read == 0)
      break;
    output->append(buffer, bytes_read);
  }

  // Let's wait for the process to finish.
  WaitForSingleObject(proc_info.hProcess, INFINITE);
  CloseHandle(proc_info.hProcess);

  return true;
}

bool KillProcess(ProcessHandle process, int exit_code, bool wait) {
  bool result = (TerminateProcess(process, exit_code) != FALSE);
  if (result && wait) {
    // The process may not end immediately due to pending I/O
    if (WAIT_OBJECT_0 != WaitForSingleObject(process, 60 * 1000))
      DLOG(ERROR) << "Error waiting for process exit: " << GetLastError();
  } else if (!result) {
    DLOG(ERROR) << "Unable to terminate process: " << GetLastError();
  }
  return result;
}

TerminationStatus GetTerminationStatus(ProcessHandle handle, int* exit_code) {
  DWORD tmp_exit_code = 0;

  if (!::GetExitCodeProcess(handle, &tmp_exit_code)) {
    NOTREACHED();
    if (exit_code) {
      // This really is a random number.  We haven't received any
      // information about the exit code, presumably because this
      // process doesn't have permission to get the exit code, or
      // because of some other cause for GetExitCodeProcess to fail
      // (MSDN docs don't give the possible failure error codes for
      // this function, so it could be anything).  But we don't want
      // to leave exit_code uninitialized, since that could cause
      // random interpretations of the exit code.  So we assume it
      // terminated "normally" in this case.
      *exit_code = kNormalTerminationExitCode;
    }
    // Assume the child has exited normally if we can't get the exit
    // code.
    return TERMINATION_STATUS_NORMAL_TERMINATION;
  }
  if (tmp_exit_code == STILL_ACTIVE) {
    DWORD wait_result = WaitForSingleObject(handle, 0);
    if (wait_result == WAIT_TIMEOUT) {
      if (exit_code)
        *exit_code = wait_result;
      return TERMINATION_STATUS_STILL_RUNNING;
    }

    DCHECK_EQ(WAIT_OBJECT_0, wait_result);

    // Strange, the process used 0x103 (STILL_ACTIVE) as exit code.
    NOTREACHED();

    return TERMINATION_STATUS_ABNORMAL_TERMINATION;
  }

  if (exit_code)
    *exit_code = tmp_exit_code;

  switch (tmp_exit_code) {
    case kNormalTerminationExitCode:
      return TERMINATION_STATUS_NORMAL_TERMINATION;
    case kDebuggerInactiveExitCode:  // STATUS_DEBUGGER_INACTIVE.
    case kKeyboardInterruptExitCode:  // Control-C/end session.
    case kDebuggerTerminatedExitCode:  // Debugger terminated process.
    case kProcessKilledExitCode:  // Task manager kill.
      return TERMINATION_STATUS_PROCESS_WAS_KILLED;
    default:
      // All other exit codes indicate crashes.
      return TERMINATION_STATUS_PROCESS_CRASHED;
  }
}

bool WaitForExitCode(ProcessHandle handle, int* exit_code) {
  bool success = WaitForExitCodeWithTimeout(handle, exit_code, INFINITE);
  CloseProcessHandle(handle);
  return success;
}

bool WaitForExitCodeWithTimeout(ProcessHandle handle, int* exit_code,
                                int64 timeout_milliseconds) {
  if (::WaitForSingleObject(handle, timeout_milliseconds) != WAIT_OBJECT_0)
    return false;
  DWORD temp_code;  // Don't clobber out-parameters in case of failure.
  if (!::GetExitCodeProcess(handle, &temp_code))
    return false;

  *exit_code = temp_code;
  return true;
}

ProcessIterator::ProcessIterator(const ProcessFilter* filter)
    : started_iteration_(false),
      filter_(filter) {
  snapshot_ = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
}

ProcessIterator::~ProcessIterator() {
  CloseHandle(snapshot_);
}

bool ProcessIterator::CheckForNextProcess() {
  InitProcessEntry(&entry_);

  if (!started_iteration_) {
    started_iteration_ = true;
    return !!Process32First(snapshot_, &entry_);
  }

  return !!Process32Next(snapshot_, &entry_);
}

void ProcessIterator::InitProcessEntry(ProcessEntry* entry) {
  memset(entry, 0, sizeof(*entry));
  entry->dwSize = sizeof(*entry);
}

bool NamedProcessIterator::IncludeEntry() {
  // Case insensitive.
  return _wcsicmp(executable_name_.c_str(), entry().exe_file()) == 0 &&
         ProcessIterator::IncludeEntry();
}

bool WaitForProcessesToExit(const std::wstring& executable_name,
                            int64 wait_milliseconds,
                            const ProcessFilter* filter) {
  const ProcessEntry* entry;
  bool result = true;
  DWORD start_time = GetTickCount();

  NamedProcessIterator iter(executable_name, filter);
  while ((entry = iter.NextProcessEntry())) {
    DWORD remaining_wait =
        std::max<int64>(0, wait_milliseconds - (GetTickCount() - start_time));
    HANDLE process = OpenProcess(SYNCHRONIZE,
                                 FALSE,
                                 entry->th32ProcessID);
    DWORD wait_result = WaitForSingleObject(process, remaining_wait);
    CloseHandle(process);
    result = result && (wait_result == WAIT_OBJECT_0);
  }

  return result;
}

bool WaitForSingleProcess(ProcessHandle handle, int64 wait_milliseconds) {
  int exit_code;
  if (!WaitForExitCodeWithTimeout(handle, &exit_code, wait_milliseconds))
    return false;
  return exit_code == 0;
}

bool CleanupProcesses(const std::wstring& executable_name,
                      int64 wait_milliseconds,
                      int exit_code,
                      const ProcessFilter* filter) {
  bool exited_cleanly = WaitForProcessesToExit(executable_name,
                                               wait_milliseconds,
                                               filter);
  if (!exited_cleanly)
    KillProcesses(executable_name, exit_code, filter);
  return exited_cleanly;
}

void EnsureProcessTerminated(ProcessHandle process) {
  DCHECK(process != GetCurrentProcess());

  // If already signaled, then we are done!
  if (WaitForSingleObject(process, 0) == WAIT_OBJECT_0) {
    CloseHandle(process);
    return;
  }

  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&TimerExpiredTask::TimedOut,
                 base::Owned(new TimerExpiredTask(process))),
      kWaitInterval);
}

///////////////////////////////////////////////////////////////////////////////
// ProcesMetrics

ProcessMetrics::ProcessMetrics(ProcessHandle process)
    : process_(process),
      processor_count_(base::SysInfo::NumberOfProcessors()),
      last_time_(0),
      last_system_time_(0) {
}

// static
ProcessMetrics* ProcessMetrics::CreateProcessMetrics(ProcessHandle process) {
  return new ProcessMetrics(process);
}

ProcessMetrics::~ProcessMetrics() { }

size_t ProcessMetrics::GetPagefileUsage() const {
  PROCESS_MEMORY_COUNTERS pmc;
  if (GetProcessMemoryInfo(process_, &pmc, sizeof(pmc))) {
    return pmc.PagefileUsage;
  }
  return 0;
}

// Returns the peak space allocated for the pagefile, in bytes.
size_t ProcessMetrics::GetPeakPagefileUsage() const {
  PROCESS_MEMORY_COUNTERS pmc;
  if (GetProcessMemoryInfo(process_, &pmc, sizeof(pmc))) {
    return pmc.PeakPagefileUsage;
  }
  return 0;
}

// Returns the current working set size, in bytes.
size_t ProcessMetrics::GetWorkingSetSize() const {
  PROCESS_MEMORY_COUNTERS pmc;
  if (GetProcessMemoryInfo(process_, &pmc, sizeof(pmc))) {
    return pmc.WorkingSetSize;
  }
  return 0;
}

// Returns the peak working set size, in bytes.
size_t ProcessMetrics::GetPeakWorkingSetSize() const {
  PROCESS_MEMORY_COUNTERS pmc;
  if (GetProcessMemoryInfo(process_, &pmc, sizeof(pmc))) {
    return pmc.PeakWorkingSetSize;
  }
  return 0;
}

bool ProcessMetrics::GetMemoryBytes(size_t* private_bytes,
                                    size_t* shared_bytes) {
  // PROCESS_MEMORY_COUNTERS_EX is not supported until XP SP2.
  // GetProcessMemoryInfo() will simply fail on prior OS. So the requested
  // information is simply not available. Hence, we will return 0 on unsupported
  // OSes. Unlike most Win32 API, we don't need to initialize the "cb" member.
  PROCESS_MEMORY_COUNTERS_EX pmcx;
  if (private_bytes &&
      GetProcessMemoryInfo(process_,
                           reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmcx),
                           sizeof(pmcx))) {
    *private_bytes = pmcx.PrivateUsage;
  }

  if (shared_bytes) {
    WorkingSetKBytes ws_usage;
    if (!GetWorkingSetKBytes(&ws_usage))
      return false;

    *shared_bytes = ws_usage.shared * 1024;
  }

  return true;
}

void ProcessMetrics::GetCommittedKBytes(CommittedKBytes* usage) const {
  MEMORY_BASIC_INFORMATION mbi = {0};
  size_t committed_private = 0;
  size_t committed_mapped = 0;
  size_t committed_image = 0;
  void* base_address = NULL;
  while (VirtualQueryEx(process_, base_address, &mbi, sizeof(mbi)) ==
      sizeof(mbi)) {
    if (mbi.State == MEM_COMMIT) {
      if (mbi.Type == MEM_PRIVATE) {
        committed_private += mbi.RegionSize;
      } else if (mbi.Type == MEM_MAPPED) {
        committed_mapped += mbi.RegionSize;
      } else if (mbi.Type == MEM_IMAGE) {
        committed_image += mbi.RegionSize;
      } else {
        NOTREACHED();
      }
    }
    void* new_base = (static_cast<BYTE*>(mbi.BaseAddress)) + mbi.RegionSize;
    // Avoid infinite loop by weird MEMORY_BASIC_INFORMATION.
    // If we query 64bit processes in a 32bit process, VirtualQueryEx()
    // returns such data.
    if (new_base <= base_address) {
      usage->image = 0;
      usage->mapped = 0;
      usage->priv = 0;
      return;
    }
    base_address = new_base;
  }
  usage->image = committed_image / 1024;
  usage->mapped = committed_mapped / 1024;
  usage->priv = committed_private / 1024;
}

bool ProcessMetrics::GetWorkingSetKBytes(WorkingSetKBytes* ws_usage) const {
  size_t ws_private = 0;
  size_t ws_shareable = 0;
  size_t ws_shared = 0;

  DCHECK(ws_usage);
  memset(ws_usage, 0, sizeof(*ws_usage));

  DWORD number_of_entries = 4096;  // Just a guess.
  PSAPI_WORKING_SET_INFORMATION* buffer = NULL;
  int retries = 5;
  for (;;) {
    DWORD buffer_size = sizeof(PSAPI_WORKING_SET_INFORMATION) +
                        (number_of_entries * sizeof(PSAPI_WORKING_SET_BLOCK));

    // if we can't expand the buffer, don't leak the previous
    // contents or pass a NULL pointer to QueryWorkingSet
    PSAPI_WORKING_SET_INFORMATION* new_buffer =
        reinterpret_cast<PSAPI_WORKING_SET_INFORMATION*>(
            realloc(buffer, buffer_size));
    if (!new_buffer) {
      free(buffer);
      return false;
    }
    buffer = new_buffer;

    // Call the function once to get number of items
    if (QueryWorkingSet(process_, buffer, buffer_size))
      break;  // Success

    if (GetLastError() != ERROR_BAD_LENGTH) {
      free(buffer);
      return false;
    }

    number_of_entries = static_cast<DWORD>(buffer->NumberOfEntries);

    // Maybe some entries are being added right now. Increase the buffer to
    // take that into account.
    number_of_entries = static_cast<DWORD>(number_of_entries * 1.25);

    if (--retries == 0) {
      free(buffer);  // If we're looping, eventually fail.
      return false;
    }
  }

  // On windows 2000 the function returns 1 even when the buffer is too small.
  // The number of entries that we are going to parse is the minimum between the
  // size we allocated and the real number of entries.
  number_of_entries =
      std::min(number_of_entries, static_cast<DWORD>(buffer->NumberOfEntries));
  for (unsigned int i = 0; i < number_of_entries; i++) {
    if (buffer->WorkingSetInfo[i].Shared) {
      ws_shareable++;
      if (buffer->WorkingSetInfo[i].ShareCount > 1)
        ws_shared++;
    } else {
      ws_private++;
    }
  }

  ws_usage->priv = ws_private * PAGESIZE_KB;
  ws_usage->shareable = ws_shareable * PAGESIZE_KB;
  ws_usage->shared = ws_shared * PAGESIZE_KB;
  free(buffer);
  return true;
}

static uint64 FileTimeToUTC(const FILETIME& ftime) {
  LARGE_INTEGER li;
  li.LowPart = ftime.dwLowDateTime;
  li.HighPart = ftime.dwHighDateTime;
  return li.QuadPart;
}

double ProcessMetrics::GetCPUUsage() {
  FILETIME now;
  FILETIME creation_time;
  FILETIME exit_time;
  FILETIME kernel_time;
  FILETIME user_time;

  GetSystemTimeAsFileTime(&now);

  if (!GetProcessTimes(process_, &creation_time, &exit_time,
                       &kernel_time, &user_time)) {
    // We don't assert here because in some cases (such as in the Task Manager)
    // we may call this function on a process that has just exited but we have
    // not yet received the notification.
    return 0;
  }
  int64 system_time = (FileTimeToUTC(kernel_time) + FileTimeToUTC(user_time)) /
                        processor_count_;
  int64 time = FileTimeToUTC(now);

  if ((last_system_time_ == 0) || (last_time_ == 0)) {
    // First call, just set the last values.
    last_system_time_ = system_time;
    last_time_ = time;
    return 0;
  }

  int64 system_time_delta = system_time - last_system_time_;
  int64 time_delta = time - last_time_;
  DCHECK_NE(0U, time_delta);
  if (time_delta == 0)
    return 0;

  // We add time_delta / 2 so the result is rounded.
  int cpu = static_cast<int>((system_time_delta * 100 + time_delta / 2) /
                             time_delta);

  last_system_time_ = system_time;
  last_time_ = time;

  return cpu;
}

bool ProcessMetrics::GetIOCounters(IoCounters* io_counters) const {
  return GetProcessIoCounters(process_, io_counters) != FALSE;
}

bool ProcessMetrics::CalculateFreeMemory(FreeMBytes* free) const {
  const SIZE_T kTopAddress = 0x7F000000;
  const SIZE_T kMegabyte = 1024 * 1024;
  SIZE_T accumulated = 0;

  MEMORY_BASIC_INFORMATION largest = {0};
  UINT_PTR scan = 0;
  while (scan < kTopAddress) {
    MEMORY_BASIC_INFORMATION info;
    if (!::VirtualQueryEx(process_, reinterpret_cast<void*>(scan),
                          &info, sizeof(info)))
      return false;
    if (info.State == MEM_FREE) {
      accumulated += info.RegionSize;
      UINT_PTR end = scan + info.RegionSize;
      if (info.RegionSize > largest.RegionSize)
        largest = info;
    }
    scan += info.RegionSize;
  }
  free->largest = largest.RegionSize / kMegabyte;
  free->largest_ptr = largest.BaseAddress;
  free->total = accumulated / kMegabyte;
  return true;
}

bool EnableLowFragmentationHeap() {
  HMODULE kernel32 = GetModuleHandle(L"kernel32.dll");
  HeapSetFn heap_set = reinterpret_cast<HeapSetFn>(GetProcAddress(
      kernel32,
      "HeapSetInformation"));

  // On Windows 2000, the function is not exported. This is not a reason to
  // fail.
  if (!heap_set)
    return true;

  unsigned number_heaps = GetProcessHeaps(0, NULL);
  if (!number_heaps)
    return false;

  // Gives us some extra space in the array in case a thread is creating heaps
  // at the same time we're querying them.
  static const int MARGIN = 8;
  scoped_array<HANDLE> heaps(new HANDLE[number_heaps + MARGIN]);
  number_heaps = GetProcessHeaps(number_heaps + MARGIN, heaps.get());
  if (!number_heaps)
    return false;

  for (unsigned i = 0; i < number_heaps; ++i) {
    ULONG lfh_flag = 2;
    // Don't bother with the result code. It may fails on heaps that have the
    // HEAP_NO_SERIALIZE flag. This is expected and not a problem at all.
    heap_set(heaps[i],
             HeapCompatibilityInformation,
             &lfh_flag,
             sizeof(lfh_flag));
  }
  return true;
}

void EnableTerminationOnHeapCorruption() {
  // Ignore the result code. Supported on XP SP3 and Vista.
  HeapSetInformation(NULL, HeapEnableTerminationOnCorruption, NULL, 0);
}

void EnableTerminationOnOutOfMemory() {
  std::set_new_handler(&OnNoMemory);
}

bool EnableInProcessStackDumping() {
  // Add stack dumping support on exception on windows. Similar to OS_POSIX
  // signal() handling in process_util_posix.cc.
  g_previous_filter = SetUnhandledExceptionFilter(&StackDumpExceptionFilter);
  AttachToConsole();
  return true;
}

void RaiseProcessToHighPriority() {
  SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
}

// GetPerformanceInfo is not available on WIN2K.  So we'll
// load it on-the-fly.
const wchar_t kPsapiDllName[] = L"psapi.dll";
typedef BOOL (WINAPI *GetPerformanceInfoFunction) (
    PPERFORMANCE_INFORMATION pPerformanceInformation,
    DWORD cb);

// Beware of races if called concurrently from multiple threads.
static BOOL InternalGetPerformanceInfo(
    PPERFORMANCE_INFORMATION pPerformanceInformation, DWORD cb) {
  static GetPerformanceInfoFunction GetPerformanceInfo_func = NULL;
  if (!GetPerformanceInfo_func) {
    HMODULE psapi_dll = ::GetModuleHandle(kPsapiDllName);
    if (psapi_dll)
      GetPerformanceInfo_func = reinterpret_cast<GetPerformanceInfoFunction>(
          GetProcAddress(psapi_dll, "GetPerformanceInfo"));

    if (!GetPerformanceInfo_func) {
      // The function could be loaded!
      memset(pPerformanceInformation, 0, cb);
      return FALSE;
    }
  }
  return GetPerformanceInfo_func(pPerformanceInformation, cb);
}

size_t GetSystemCommitCharge() {
  // Get the System Page Size.
  SYSTEM_INFO system_info;
  GetSystemInfo(&system_info);

  PERFORMANCE_INFORMATION info;
  if (!InternalGetPerformanceInfo(&info, sizeof(info))) {
    DLOG(ERROR) << "Failed to fetch internal performance info.";
    return 0;
  }
  return (info.CommitTotal * system_info.dwPageSize) / 1024;
}

}  // namespace base
