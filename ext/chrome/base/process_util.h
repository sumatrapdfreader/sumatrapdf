// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file/namespace contains utility functions for enumerating, ending and
// computing statistics of processes.

#ifndef BASE_PROCESS_UTIL_H_
#define BASE_PROCESS_UTIL_H_
#pragma once

#include "base/basictypes.h"

#if defined(OS_WIN)
#include <windows.h>
#include <tlhelp32.h>
#elif defined(OS_MACOSX) || defined(OS_BSD)
// kinfo_proc is defined in <sys/sysctl.h>, but this forward declaration
// is sufficient for the vector<kinfo_proc> below.
struct kinfo_proc;
// malloc_zone_t is defined in <malloc/malloc.h>, but this forward declaration
// is sufficient for GetPurgeableZone() below.
typedef struct _malloc_zone_t malloc_zone_t;
#if !defined(OS_BSD)
#include <mach/mach.h>
#endif
#elif defined(OS_POSIX)
#include <dirent.h>
#include <limits.h>
#include <sys/types.h>
#endif

#include <list>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/base_export.h"
#include "base/file_descriptor_shuffle.h"
#include "base/file_path.h"
#include "base/process.h"

class CommandLine;

namespace base {

#if defined(OS_WIN)
struct ProcessEntry : public PROCESSENTRY32 {
  ProcessId pid() const { return th32ProcessID; }
  ProcessId parent_pid() const { return th32ParentProcessID; }
  const wchar_t* exe_file() const { return szExeFile; }
};

struct IoCounters : public IO_COUNTERS {
};

// Process access masks. These constants provide platform-independent
// definitions for the standard Windows access masks.
// See http://msdn.microsoft.com/en-us/library/ms684880(VS.85).aspx for
// the specific semantics of each mask value.
const uint32 kProcessAccessTerminate              = PROCESS_TERMINATE;
const uint32 kProcessAccessCreateThread           = PROCESS_CREATE_THREAD;
const uint32 kProcessAccessSetSessionId           = PROCESS_SET_SESSIONID;
const uint32 kProcessAccessVMOperation            = PROCESS_VM_OPERATION;
const uint32 kProcessAccessVMRead                 = PROCESS_VM_READ;
const uint32 kProcessAccessVMWrite                = PROCESS_VM_WRITE;
const uint32 kProcessAccessDuplicateHandle        = PROCESS_DUP_HANDLE;
const uint32 kProcessAccessCreateProcess          = PROCESS_CREATE_PROCESS;
const uint32 kProcessAccessSetQuota               = PROCESS_SET_QUOTA;
const uint32 kProcessAccessSetInformation         = PROCESS_SET_INFORMATION;
const uint32 kProcessAccessQueryInformation       = PROCESS_QUERY_INFORMATION;
const uint32 kProcessAccessSuspendResume          = PROCESS_SUSPEND_RESUME;
const uint32 kProcessAccessQueryLimitedInfomation =
    PROCESS_QUERY_LIMITED_INFORMATION;
const uint32 kProcessAccessWaitForTermination     = SYNCHRONIZE;
#elif defined(OS_POSIX)

struct ProcessEntry {
  ProcessEntry();
  ~ProcessEntry();

  ProcessId pid() const { return pid_; }
  ProcessId parent_pid() const { return ppid_; }
  ProcessId gid() const { return gid_; }
  const char* exe_file() const { return exe_file_.c_str(); }
  const std::vector<std::string>& cmd_line_args() const {
    return cmd_line_args_;
  }

  ProcessId pid_;
  ProcessId ppid_;
  ProcessId gid_;
  std::string exe_file_;
  std::vector<std::string> cmd_line_args_;
};

struct IoCounters {
  uint64_t ReadOperationCount;
  uint64_t WriteOperationCount;
  uint64_t OtherOperationCount;
  uint64_t ReadTransferCount;
  uint64_t WriteTransferCount;
  uint64_t OtherTransferCount;
};

// Process access masks. They are not used on Posix because access checking
// does not happen during handle creation.
const uint32 kProcessAccessTerminate              = 0;
const uint32 kProcessAccessCreateThread           = 0;
const uint32 kProcessAccessSetSessionId           = 0;
const uint32 kProcessAccessVMOperation            = 0;
const uint32 kProcessAccessVMRead                 = 0;
const uint32 kProcessAccessVMWrite                = 0;
const uint32 kProcessAccessDuplicateHandle        = 0;
const uint32 kProcessAccessCreateProcess          = 0;
const uint32 kProcessAccessSetQuota               = 0;
const uint32 kProcessAccessSetInformation         = 0;
const uint32 kProcessAccessQueryInformation       = 0;
const uint32 kProcessAccessSuspendResume          = 0;
const uint32 kProcessAccessQueryLimitedInfomation = 0;
const uint32 kProcessAccessWaitForTermination     = 0;
#endif  // defined(OS_POSIX)

// Return status values from GetTerminationStatus.  Don't use these as
// exit code arguments to KillProcess*(), use platform/application
// specific values instead.
enum TerminationStatus {
  TERMINATION_STATUS_NORMAL_TERMINATION,   // zero exit status
  TERMINATION_STATUS_ABNORMAL_TERMINATION, // non-zero exit status
  TERMINATION_STATUS_PROCESS_WAS_KILLED,   // e.g. SIGKILL or task manager kill
  TERMINATION_STATUS_PROCESS_CRASHED,      // e.g. Segmentation fault
  TERMINATION_STATUS_STILL_RUNNING,        // child hasn't exited yet
  TERMINATION_STATUS_MAX_ENUM
};

// Returns the id of the current process.
BASE_EXPORT ProcessId GetCurrentProcId();

// Returns the ProcessHandle of the current process.
BASE_EXPORT ProcessHandle GetCurrentProcessHandle();

// Converts a PID to a process handle. This handle must be closed by
// CloseProcessHandle when you are done with it. Returns true on success.
BASE_EXPORT bool OpenProcessHandle(ProcessId pid, ProcessHandle* handle);

// Converts a PID to a process handle. On Windows the handle is opened
// with more access rights and must only be used by trusted code.
// You have to close returned handle using CloseProcessHandle. Returns true
// on success.
// TODO(sanjeevr): Replace all calls to OpenPrivilegedProcessHandle with the
// more specific OpenProcessHandleWithAccess method and delete this.
BASE_EXPORT bool OpenPrivilegedProcessHandle(ProcessId pid,
                                             ProcessHandle* handle);

// Converts a PID to a process handle using the desired access flags. Use a
// combination of the kProcessAccess* flags defined above for |access_flags|.
BASE_EXPORT bool OpenProcessHandleWithAccess(ProcessId pid,
                                             uint32 access_flags,
                                             ProcessHandle* handle);

// Closes the process handle opened by OpenProcessHandle.
BASE_EXPORT void CloseProcessHandle(ProcessHandle process);

// Returns the unique ID for the specified process. This is functionally the
// same as Windows' GetProcessId(), but works on versions of Windows before
// Win XP SP1 as well.
BASE_EXPORT ProcessId GetProcId(ProcessHandle process);

#if defined(OS_LINUX) || defined(OS_ANDROID) || defined(OS_BSD)
// Returns the path to the executable of the given process.
BASE_EXPORT FilePath GetProcessExecutablePath(ProcessHandle process);
#endif

#if defined(OS_LINUX) || defined(OS_ANDROID)
// Parse the data found in /proc/<pid>/stat and return the sum of the
// CPU-related ticks.  Returns -1 on parse error.
// Exposed for testing.
BASE_EXPORT int ParseProcStatCPU(const std::string& input);

// The maximum allowed value for the OOM score.
const int kMaxOomScore = 1000;

// This adjusts /proc/<pid>/oom_score_adj so the Linux OOM killer will
// prefer to kill certain process types over others. The range for the
// adjustment is [-1000, 1000], with [0, 1000] being user accessible.
// If the Linux system doesn't support the newer oom_score_adj range
// of [0, 1000], then we revert to using the older oom_adj, and
// translate the given value into [0, 15].  Some aliasing of values
// may occur in that case, of course.
BASE_EXPORT bool AdjustOOMScore(ProcessId process, int score);
#endif  // defined(OS_LINUX) || defined(OS_ANDROID)

#if defined(OS_POSIX)
// Returns the ID for the parent of the given process.
BASE_EXPORT ProcessId GetParentProcessId(ProcessHandle process);

// Close all file descriptors, except those which are a destination in the
// given multimap. Only call this function in a child process where you know
// that there aren't any other threads.
BASE_EXPORT void CloseSuperfluousFds(const InjectiveMultimap& saved_map);
#endif  // defined(OS_POSIX)

// TODO(evan): rename these to use StudlyCaps.
typedef std::vector<std::pair<std::string, std::string> > environment_vector;
typedef std::vector<std::pair<int, int> > file_handle_mapping_vector;

#if defined(OS_MACOSX)
// Used with LaunchOptions::synchronize and LaunchSynchronize, a
// LaunchSynchronizationHandle is an opaque value that LaunchProcess will
// create and set, and that LaunchSynchronize will consume and destroy.
typedef int* LaunchSynchronizationHandle;
#endif  // defined(OS_MACOSX)

// Options for launching a subprocess that are passed to LaunchProcess().
// The default constructor constructs the object with default options.
struct LaunchOptions {
  LaunchOptions() : wait(false),
#if defined(OS_WIN)
                    start_hidden(false), inherit_handles(false), as_user(NULL),
                    empty_desktop_name(false), job_handle(NULL)
#else
                    environ(NULL), fds_to_remap(NULL), maximize_rlimits(NULL),
                    new_process_group(false)
#if defined(OS_LINUX)
                  , clone_flags(0)
#endif  // OS_LINUX
#if defined(OS_CHROMEOS)
                  , ctrl_terminal_fd(-1)
#endif  // OS_CHROMEOS
#if defined(OS_MACOSX)
                  , synchronize(NULL)
#endif  // defined(OS_MACOSX)
#endif  // !defined(OS_WIN)
      {}

  // If true, wait for the process to complete.
  bool wait;

#if defined(OS_WIN)
  bool start_hidden;

  // If true, the new process inherits handles from the parent.
  bool inherit_handles;

  // If non-NULL, runs as if the user represented by the token had launched it.
  // Whether the application is visible on the interactive desktop depends on
  // the token belonging to an interactive logon session.
  //
  // To avoid hard to diagnose problems, when specified this loads the
  // environment variables associated with the user and if this operation fails
  // the entire call fails as well.
  UserTokenHandle as_user;

  // If true, use an empty string for the desktop name.
  bool empty_desktop_name;

  // If non-NULL, launches the application in that job object.
  HANDLE job_handle;
#else
  // If non-NULL, set/unset environment variables.
  // See documentation of AlterEnvironment().
  // This pointer is owned by the caller and must live through the
  // call to LaunchProcess().
  const environment_vector* environ;

  // If non-NULL, remap file descriptors according to the mapping of
  // src fd->dest fd to propagate FDs into the child process.
  // This pointer is owned by the caller and must live through the
  // call to LaunchProcess().
  const file_handle_mapping_vector* fds_to_remap;

  // Each element is an RLIMIT_* constant that should be raised to its
  // rlim_max.  This pointer is owned by the caller and must live through
  // the call to LaunchProcess().
  const std::set<int>* maximize_rlimits;

  // If true, start the process in a new process group, instead of
  // inheriting the parent's process group.  The pgid of the child process
  // will be the same as its pid.
  bool new_process_group;

#if defined(OS_LINUX)
  // If non-zero, start the process using clone(), using flags as provided.
  int clone_flags;
#endif  // defined(OS_LINUX)

#if defined(OS_CHROMEOS)
  // If non-negative, the specified file descriptor will be set as the launched
  // process' controlling terminal.
  int ctrl_terminal_fd;
#endif  // defined(OS_CHROMEOS)

#if defined(OS_MACOSX)
  // When non-NULL, a new LaunchSynchronizationHandle will be created and
  // stored in *synchronize whenever LaunchProcess returns true in the parent
  // process. The child process will have been created (with fork) but will
  // be waiting (before exec) for the parent to call LaunchSynchronize with
  // this handle. Only when LaunchSynchronize is called will the child be
  // permitted to continue execution and call exec. LaunchSynchronize
  // destroys the handle created by LaunchProcess.
  //
  // When synchronize is non-NULL, the parent must call LaunchSynchronize
  // whenever LaunchProcess returns true. No exceptions.
  //
  // Synchronization is useful when the parent process needs to guarantee that
  // it can take some action (such as recording the newly-forked child's
  // process ID) before the child does something (such as using its process ID
  // to communicate with its parent).
  //
  // |synchronize| and |wait| must not both be set simultaneously.
  LaunchSynchronizationHandle* synchronize;
#endif  // defined(OS_MACOSX)

#endif  // !defined(OS_WIN)
};

// Launch a process via the command line |cmdline|.
// See the documentation of LaunchOptions for details on |options|.
//
// If |process_handle| is non-NULL, it will be filled in with the
// handle of the launched process.  NOTE: In this case, the caller is
// responsible for closing the handle so that it doesn't leak!
// Otherwise, the process handle will be implicitly closed.
//
// Unix-specific notes:
// - All file descriptors open in the parent process will be closed in the
//   child process except for any preserved by options::fds_to_remap, and
//   stdin, stdout, and stderr. If not remapped by options::fds_to_remap,
//   stdin is reopened as /dev/null, and the child is allowed to inherit its
//   parent's stdout and stderr.
// - If the first argument on the command line does not contain a slash,
//   PATH will be searched.  (See man execvp.)
BASE_EXPORT bool LaunchProcess(const CommandLine& cmdline,
                               const LaunchOptions& options,
                               ProcessHandle* process_handle);

#if defined(OS_WIN)

enum IntegrityLevel {
  INTEGRITY_UNKNOWN,
  LOW_INTEGRITY,
  MEDIUM_INTEGRITY,
  HIGH_INTEGRITY,
};
// Determine the integrity level of the specified process. Returns false
// if the system does not support integrity levels (pre-Vista) or in the case
// of an underlying system failure.
BASE_EXPORT bool GetProcessIntegrityLevel(ProcessHandle process,
                                          IntegrityLevel *level);

// Windows-specific LaunchProcess that takes the command line as a
// string.  Useful for situations where you need to control the
// command line arguments directly, but prefer the CommandLine version
// if launching Chrome itself.
//
// The first command line argument should be the path to the process,
// and don't forget to quote it.
//
// Example (including literal quotes)
//  cmdline = "c:\windows\explorer.exe" -foo "c:\bar\"
BASE_EXPORT bool LaunchProcess(const string16& cmdline,
                               const LaunchOptions& options,
                               ProcessHandle* process_handle);

#elif defined(OS_POSIX)
// A POSIX-specific version of LaunchProcess that takes an argv array
// instead of a CommandLine.  Useful for situations where you need to
// control the command line arguments directly, but prefer the
// CommandLine version if launching Chrome itself.
BASE_EXPORT bool LaunchProcess(const std::vector<std::string>& argv,
                               const LaunchOptions& options,
                               ProcessHandle* process_handle);

// AlterEnvironment returns a modified environment vector, constructed from the
// given environment and the list of changes given in |changes|. Each key in
// the environment is matched against the first element of the pairs. In the
// event of a match, the value is replaced by the second of the pair, unless
// the second is empty, in which case the key-value is removed.
//
// The returned array is allocated using new[] and must be freed by the caller.
BASE_EXPORT char** AlterEnvironment(const environment_vector& changes,
                                    const char* const* const env);

#if defined(OS_MACOSX)

// After a successful call to LaunchProcess with LaunchOptions::synchronize
// set, the parent process must call LaunchSynchronize to allow the child
// process to proceed, and to destroy the LaunchSynchronizationHandle.
BASE_EXPORT void LaunchSynchronize(LaunchSynchronizationHandle handle);

#endif  // defined(OS_MACOSX)
#endif  // defined(OS_POSIX)

#if defined(OS_WIN)
// Set JOBOBJECT_EXTENDED_LIMIT_INFORMATION to JobObject |job_object|.
// As its limit_info.BasicLimitInformation.LimitFlags has
// JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE.
// When the provide JobObject |job_object| is closed, the binded process will
// be terminated.
BASE_EXPORT bool SetJobObjectAsKillOnJobClose(HANDLE job_object);
#endif  // defined(OS_WIN)

// Executes the application specified by |cl| and wait for it to exit. Stores
// the output (stdout) in |output|. Redirects stderr to /dev/null. Returns true
// on success (application launched and exited cleanly, with exit code
// indicating success).
BASE_EXPORT bool GetAppOutput(const CommandLine& cl, std::string* output);

#if defined(OS_POSIX)
// A restricted version of |GetAppOutput()| which (a) clears the environment,
// and (b) stores at most |max_output| bytes; also, it doesn't search the path
// for the command.
BASE_EXPORT bool GetAppOutputRestricted(const CommandLine& cl,
                                        std::string* output, size_t max_output);

// A version of |GetAppOutput()| which also returns the exit code of the
// executed command. Returns true if the application runs and exits cleanly. If
// this is the case the exit code of the application is available in
// |*exit_code|.
BASE_EXPORT bool GetAppOutputWithExitCode(const CommandLine& cl,
                                          std::string* output, int* exit_code);
#endif  // defined(OS_POSIX)

// Used to filter processes by process ID.
class ProcessFilter {
 public:
  // Returns true to indicate set-inclusion and false otherwise.  This method
  // should not have side-effects and should be idempotent.
  virtual bool Includes(const ProcessEntry& entry) const = 0;

 protected:
  virtual ~ProcessFilter() {}
};

// Returns the number of processes on the machine that are running from the
// given executable name.  If filter is non-null, then only processes selected
// by the filter will be counted.
BASE_EXPORT int GetProcessCount(const FilePath::StringType& executable_name,
                                const ProcessFilter* filter);

// Attempts to kill all the processes on the current machine that were launched
// from the given executable name, ending them with the given exit code.  If
// filter is non-null, then only processes selected by the filter are killed.
// Returns true if all processes were able to be killed off, false if at least
// one couldn't be killed.
BASE_EXPORT bool KillProcesses(const FilePath::StringType& executable_name,
                               int exit_code, const ProcessFilter* filter);

// Attempts to kill the process identified by the given process
// entry structure, giving it the specified exit code. If |wait| is true, wait
// for the process to be actually terminated before returning.
// Returns true if this is successful, false otherwise.
BASE_EXPORT bool KillProcess(ProcessHandle process, int exit_code, bool wait);

#if defined(OS_POSIX)
// Attempts to kill the process group identified by |process_group_id|. Returns
// true on success.
BASE_EXPORT bool KillProcessGroup(ProcessHandle process_group_id);
#endif  // defined(OS_POSIX)

#if defined(OS_WIN)
BASE_EXPORT bool KillProcessById(ProcessId process_id, int exit_code,
                                 bool wait);
#endif  // defined(OS_WIN)

// Get the termination status of the process by interpreting the
// circumstances of the child process' death. |exit_code| is set to
// the status returned by waitpid() on POSIX, and from
// GetExitCodeProcess() on Windows.  |exit_code| may be NULL if the
// caller is not interested in it.  Note that on Linux, this function
// will only return a useful result the first time it is called after
// the child exits (because it will reap the child and the information
// will no longer be available).
BASE_EXPORT TerminationStatus GetTerminationStatus(ProcessHandle handle,
                                                   int* exit_code);

// Waits for process to exit. On POSIX systems, if the process hasn't been
// signaled then puts the exit code in |exit_code|; otherwise it's considered
// a failure. On Windows |exit_code| is always filled. Returns true on success,
// and closes |handle| in any case.
BASE_EXPORT bool WaitForExitCode(ProcessHandle handle, int* exit_code);

// Waits for process to exit. If it did exit within |timeout_milliseconds|,
// then puts the exit code in |exit_code|, and returns true.
// In POSIX systems, if the process has been signaled then |exit_code| is set
// to -1. Returns false on failure (the caller is then responsible for closing
// |handle|).
// The caller is always responsible for closing the |handle|.
BASE_EXPORT bool WaitForExitCodeWithTimeout(ProcessHandle handle,
                                            int* exit_code,
                                            int64 timeout_milliseconds);

// Wait for all the processes based on the named executable to exit.  If filter
// is non-null, then only processes selected by the filter are waited on.
// Returns after all processes have exited or wait_milliseconds have expired.
// Returns true if all the processes exited, false otherwise.
BASE_EXPORT bool WaitForProcessesToExit(
    const FilePath::StringType& executable_name,
    int64 wait_milliseconds,
    const ProcessFilter* filter);

// Wait for a single process to exit. Return true if it exited cleanly within
// the given time limit. On Linux |handle| must be a child process, however
// on Mac and Windows it can be any process.
BASE_EXPORT bool WaitForSingleProcess(ProcessHandle handle,
                                      int64 wait_milliseconds);

// Waits a certain amount of time (can be 0) for all the processes with a given
// executable name to exit, then kills off any of them that are still around.
// If filter is non-null, then only processes selected by the filter are waited
// on.  Killed processes are ended with the given exit code.  Returns false if
// any processes needed to be killed, true if they all exited cleanly within
// the wait_milliseconds delay.
BASE_EXPORT bool CleanupProcesses(const FilePath::StringType& executable_name,
                                  int64 wait_milliseconds,
                                  int exit_code,
                                  const ProcessFilter* filter);

// This method ensures that the specified process eventually terminates, and
// then it closes the given process handle.
//
// It assumes that the process has already been signalled to exit, and it
// begins by waiting a small amount of time for it to exit.  If the process
// does not appear to have exited, then this function starts to become
// aggressive about ensuring that the process terminates.
//
// On Linux this method does not block the calling thread.
// On OS X this method may block for up to 2 seconds.
//
// NOTE: The process handle must have been opened with the PROCESS_TERMINATE
// and SYNCHRONIZE permissions.
//
BASE_EXPORT void EnsureProcessTerminated(ProcessHandle process_handle);

#if defined(OS_POSIX) && !defined(OS_MACOSX)
// The nicer version of EnsureProcessTerminated() that is patient and will
// wait for |process_handle| to finish and then reap it.
BASE_EXPORT void EnsureProcessGetsReaped(ProcessHandle process_handle);
#endif

// This class provides a way to iterate through a list of processes on the
// current machine with a specified filter.
// To use, create an instance and then call NextProcessEntry() until it returns
// false.
class BASE_EXPORT ProcessIterator {
 public:
  typedef std::list<ProcessEntry> ProcessEntries;

  explicit ProcessIterator(const ProcessFilter* filter);
  virtual ~ProcessIterator();

  // If there's another process that matches the given executable name,
  // returns a const pointer to the corresponding PROCESSENTRY32.
  // If there are no more matching processes, returns NULL.
  // The returned pointer will remain valid until NextProcessEntry()
  // is called again or this NamedProcessIterator goes out of scope.
  const ProcessEntry* NextProcessEntry();

  // Takes a snapshot of all the ProcessEntry found.
  ProcessEntries Snapshot();

 protected:
  virtual bool IncludeEntry();
  const ProcessEntry& entry() { return entry_; }

 private:
  // Determines whether there's another process (regardless of executable)
  // left in the list of all processes.  Returns true and sets entry_ to
  // that process's info if there is one, false otherwise.
  bool CheckForNextProcess();

  // Initializes a PROCESSENTRY32 data structure so that it's ready for
  // use with Process32First/Process32Next.
  void InitProcessEntry(ProcessEntry* entry);

#if defined(OS_WIN)
  HANDLE snapshot_;
  bool started_iteration_;
#elif defined(OS_MACOSX) || defined(OS_BSD)
  std::vector<kinfo_proc> kinfo_procs_;
  size_t index_of_kinfo_proc_;
#elif defined(OS_POSIX)
  DIR *procfs_dir_;
#endif
  ProcessEntry entry_;
  const ProcessFilter* filter_;

  DISALLOW_COPY_AND_ASSIGN(ProcessIterator);
};

// This class provides a way to iterate through the list of processes
// on the current machine that were started from the given executable
// name.  To use, create an instance and then call NextProcessEntry()
// until it returns false.
class BASE_EXPORT NamedProcessIterator : public ProcessIterator {
 public:
  NamedProcessIterator(const FilePath::StringType& executable_name,
                       const ProcessFilter* filter);
  virtual ~NamedProcessIterator();

 protected:
  virtual bool IncludeEntry() OVERRIDE;

 private:
  FilePath::StringType executable_name_;

  DISALLOW_COPY_AND_ASSIGN(NamedProcessIterator);
};

// Working Set (resident) memory usage broken down by
//
// On Windows:
// priv (private): These pages (kbytes) cannot be shared with any other process.
// shareable:      These pages (kbytes) can be shared with other processes under
//                 the right circumstances.
// shared :        These pages (kbytes) are currently shared with at least one
//                 other process.
//
// On Linux:
// priv:           Pages mapped only by this process
// shared:         PSS or 0 if the kernel doesn't support this
// shareable:      0
//
// On OS X: TODO(thakis): Revise.
// priv:           Memory.
// shared:         0
// shareable:      0
struct WorkingSetKBytes {
  WorkingSetKBytes() : priv(0), shareable(0), shared(0) {}
  size_t priv;
  size_t shareable;
  size_t shared;
};

// Committed (resident + paged) memory usage broken down by
// private: These pages cannot be shared with any other process.
// mapped:  These pages are mapped into the view of a section (backed by
//          pagefile.sys)
// image:   These pages are mapped into the view of an image section (backed by
//          file system)
struct CommittedKBytes {
  CommittedKBytes() : priv(0), mapped(0), image(0) {}
  size_t priv;
  size_t mapped;
  size_t image;
};

// Free memory (Megabytes marked as free) in the 2G process address space.
// total : total amount in megabytes marked as free. Maximum value is 2048.
// largest : size of the largest contiguous amount of memory found. It is
//   always smaller or equal to FreeMBytes::total.
// largest_ptr: starting address of the largest memory block.
struct FreeMBytes {
  size_t total;
  size_t largest;
  void* largest_ptr;
};

// Convert a POSIX timeval to microseconds.
BASE_EXPORT int64 TimeValToMicroseconds(const struct timeval& tv);

// Provides performance metrics for a specified process (CPU usage, memory and
// IO counters). To use it, invoke CreateProcessMetrics() to get an instance
// for a specific process, then access the information with the different get
// methods.
class BASE_EXPORT ProcessMetrics {
 public:
  ~ProcessMetrics();

  // Creates a ProcessMetrics for the specified process.
  // The caller owns the returned object.
#if !defined(OS_MACOSX)
  static ProcessMetrics* CreateProcessMetrics(ProcessHandle process);
#else
  class PortProvider {
   public:
    // Should return the mach task for |process| if possible, or else
    // |MACH_PORT_NULL|. Only processes that this returns tasks for will have
    // metrics on OS X (except for the current process, which always gets
    // metrics).
    virtual mach_port_t TaskForPid(ProcessHandle process) const = 0;
  };

  // The port provider needs to outlive the ProcessMetrics object returned by
  // this function. If NULL is passed as provider, the returned object
  // only returns valid metrics if |process| is the current process.
  static ProcessMetrics* CreateProcessMetrics(ProcessHandle process,
                                              PortProvider* port_provider);
#endif  // !defined(OS_MACOSX)

  // Returns the current space allocated for the pagefile, in bytes (these pages
  // may or may not be in memory).  On Linux, this returns the total virtual
  // memory size.
  size_t GetPagefileUsage() const;
  // Returns the peak space allocated for the pagefile, in bytes.
  size_t GetPeakPagefileUsage() const;
  // Returns the current working set size, in bytes.  On Linux, this returns
  // the resident set size.
  size_t GetWorkingSetSize() const;
  // Returns the peak working set size, in bytes.
  size_t GetPeakWorkingSetSize() const;
  // Returns private and sharedusage, in bytes. Private bytes is the amount of
  // memory currently allocated to a process that cannot be shared. Returns
  // false on platform specific error conditions.  Note: |private_bytes|
  // returns 0 on unsupported OSes: prior to XP SP2.
  bool GetMemoryBytes(size_t* private_bytes,
                      size_t* shared_bytes);
  // Fills a CommittedKBytes with both resident and paged
  // memory usage as per definition of CommittedBytes.
  void GetCommittedKBytes(CommittedKBytes* usage) const;
  // Fills a WorkingSetKBytes containing resident private and shared memory
  // usage in bytes, as per definition of WorkingSetBytes.
  bool GetWorkingSetKBytes(WorkingSetKBytes* ws_usage) const;

  // Computes the current process available memory for allocation.
  // It does a linear scan of the address space querying each memory region
  // for its free (unallocated) status. It is useful for estimating the memory
  // load and fragmentation.
  bool CalculateFreeMemory(FreeMBytes* free) const;

  // Returns the CPU usage in percent since the last time this method was
  // called. The first time this method is called it returns 0 and will return
  // the actual CPU info on subsequent calls.
  // On Windows, the CPU usage value is for all CPUs. So if you have 2 CPUs and
  // your process is using all the cycles of 1 CPU and not the other CPU, this
  // method returns 50.
  double GetCPUUsage();

  // Retrieves accounting information for all I/O operations performed by the
  // process.
  // If IO information is retrieved successfully, the function returns true
  // and fills in the IO_COUNTERS passed in. The function returns false
  // otherwise.
  bool GetIOCounters(IoCounters* io_counters) const;

 private:
#if !defined(OS_MACOSX)
  explicit ProcessMetrics(ProcessHandle process);
#else
  ProcessMetrics(ProcessHandle process, PortProvider* port_provider);
#endif  // defined(OS_MACOSX)

  ProcessHandle process_;

  int processor_count_;

  // Used to store the previous times and CPU usage counts so we can
  // compute the CPU usage between calls.
  int64 last_time_;
  int64 last_system_time_;

#if defined(OS_MACOSX)
  // Queries the port provider if it's set.
  mach_port_t TaskForPid(ProcessHandle process) const;

  PortProvider* port_provider_;
#elif defined(OS_POSIX)
  // Jiffie count at the last_time_ we updated.
  int last_cpu_;
#endif  // defined(OS_POSIX)

  DISALLOW_COPY_AND_ASSIGN(ProcessMetrics);
};

#if defined(OS_LINUX) || defined(OS_ANDROID)
// Data from /proc/meminfo about system-wide memory consumption.
// Values are in KB.
struct SystemMemoryInfoKB {
  SystemMemoryInfoKB() : total(0), free(0), buffers(0), cached(0),
      active_anon(0), inactive_anon(0), shmem(0) {}
  int total;
  int free;
  int buffers;
  int cached;
  int active_anon;
  int inactive_anon;
  int shmem;
};
// Retrieves data from /proc/meminfo about system-wide memory consumption.
// Fills in the provided |meminfo| structure. Returns true on success.
// Exposed for memory debugging widget.
BASE_EXPORT bool GetSystemMemoryInfo(SystemMemoryInfoKB* meminfo);
#endif  // defined(OS_LINUX) || defined(OS_ANDROID)

// Returns the memory committed by the system in KBytes.
// Returns 0 if it can't compute the commit charge.
BASE_EXPORT size_t GetSystemCommitCharge();

// Enables low fragmentation heap (LFH) for every heaps of this process. This
// won't have any effect on heaps created after this function call. It will not
// modify data allocated in the heaps before calling this function. So it is
// better to call this function early in initialization and again before
// entering the main loop.
// Note: Returns true on Windows 2000 without doing anything.
BASE_EXPORT bool EnableLowFragmentationHeap();

// Enables 'terminate on heap corruption' flag. Helps protect against heap
// overflow. Has no effect if the OS doesn't provide the necessary facility.
BASE_EXPORT void EnableTerminationOnHeapCorruption();

// Turns on process termination if memory runs out.
BASE_EXPORT void EnableTerminationOnOutOfMemory();

#if defined(OS_MACOSX)
// Exposed for testing.
BASE_EXPORT malloc_zone_t* GetPurgeableZone();
#endif  // defined(OS_MACOSX)

// Enables stack dump to console output on exception and signals.
// When enabled, the process will quit immediately. This is meant to be used in
// unit_tests only!
BASE_EXPORT bool EnableInProcessStackDumping();

// If supported on the platform, and the user has sufficent rights, increase
// the current process's scheduling priority to a high priority.
BASE_EXPORT void RaiseProcessToHighPriority();

#if defined(OS_MACOSX)
// Restore the default exception handler, setting it to Apple Crash Reporter
// (ReportCrash).  When forking and execing a new process, the child will
// inherit the parent's exception ports, which may be set to the Breakpad
// instance running inside the parent.  The parent's Breakpad instance should
// not handle the child's exceptions.  Calling RestoreDefaultExceptionHandler
// in the child after forking will restore the standard exception handler.
// See http://crbug.com/20371/ for more details.
void RestoreDefaultExceptionHandler();
#endif  // defined(OS_MACOSX)

}  // namespace base

#endif  // BASE_PROCESS_UTIL_H_
