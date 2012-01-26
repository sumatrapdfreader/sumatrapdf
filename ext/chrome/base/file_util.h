// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains utility functions for dealing with the local
// filesystem.

#ifndef BASE_FILE_UTIL_H_
#define BASE_FILE_UTIL_H_
#pragma once

#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#elif defined(OS_POSIX)
#include <sys/stat.h>
#include <unistd.h>
#endif

#include <stdio.h>

#include <set>
#include <stack>
#include <string>
#include <vector>

#include "base/base_export.h"
#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/platform_file.h"
#include "base/string16.h"

#if defined(OS_POSIX)
#include "base/eintr_wrapper.h"
#include "base/file_descriptor_posix.h"
#include "base/logging.h"
#endif

namespace base {
class Time;
}

namespace file_util {

//-----------------------------------------------------------------------------
// Functions that operate purely on a path string w/o touching the filesystem:

// Returns true if the given path ends with a path separator character.
BASE_EXPORT bool EndsWithSeparator(const FilePath& path);

// Makes sure that |path| ends with a separator IFF path is a directory that
// exists. Returns true if |path| is an existing directory, false otherwise.
BASE_EXPORT bool EnsureEndsWithSeparator(FilePath* path);

// Convert provided relative path into an absolute path.  Returns false on
// error. On POSIX, this function fails if the path does not exist.
BASE_EXPORT bool AbsolutePath(FilePath* path);

// Returns true if |parent| contains |child|. Both paths are converted to
// absolute paths before doing the comparison.
BASE_EXPORT bool ContainsPath(const FilePath& parent, const FilePath& child);

//-----------------------------------------------------------------------------
// Functions that involve filesystem access or modification:

// Returns the number of files matching the current path that were
// created on or after the given |file_time|.  Doesn't count ".." or ".".
//
// Note for POSIX environments: a file created before |file_time|
// can be mis-detected as a newer file due to low precision of
// timestmap of file creation time. If you need to avoid such
// mis-detection perfectly, you should wait one second before
// obtaining |file_time|.
BASE_EXPORT int CountFilesCreatedAfter(const FilePath& path,
                                       const base::Time& file_time);

// Returns the total number of bytes used by all the files under |root_path|.
// If the path does not exist the function returns 0.
//
// This function is implemented using the FileEnumerator class so it is not
// particularly speedy in any platform.
BASE_EXPORT int64 ComputeDirectorySize(const FilePath& root_path);

// Returns the total number of bytes used by all files matching the provided
// |pattern|, on this |directory| (without recursion). If the path does not
// exist the function returns 0.
//
// This function is implemented using the FileEnumerator class so it is not
// particularly speedy in any platform.
BASE_EXPORT int64 ComputeFilesSize(const FilePath& directory,
                                   const FilePath::StringType& pattern);

// Deletes the given path, whether it's a file or a directory.
// If it's a directory, it's perfectly happy to delete all of the
// directory's contents.  Passing true to recursive deletes
// subdirectories and their contents as well.
// Returns true if successful, false otherwise.
//
// WARNING: USING THIS WITH recursive==true IS EQUIVALENT
//          TO "rm -rf", SO USE WITH CAUTION.
BASE_EXPORT bool Delete(const FilePath& path, bool recursive);

#if defined(OS_WIN)
// Schedules to delete the given path, whether it's a file or a directory, until
// the operating system is restarted.
// Note:
// 1) The file/directory to be deleted should exist in a temp folder.
// 2) The directory to be deleted must be empty.
BASE_EXPORT bool DeleteAfterReboot(const FilePath& path);
#endif

// Moves the given path, whether it's a file or a directory.
// If a simple rename is not possible, such as in the case where the paths are
// on different volumes, this will attempt to copy and delete. Returns
// true for success.
BASE_EXPORT bool Move(const FilePath& from_path, const FilePath& to_path);

// Renames file |from_path| to |to_path|. Both paths must be on the same
// volume, or the function will fail. Destination file will be created
// if it doesn't exist. Prefer this function over Move when dealing with
// temporary files. On Windows it preserves attributes of the target file.
// Returns true on success.
BASE_EXPORT bool ReplaceFile(const FilePath& from_path,
                             const FilePath& to_path);

// Copies a single file. Use CopyDirectory to copy directories.
BASE_EXPORT bool CopyFile(const FilePath& from_path, const FilePath& to_path);

// Copies the given path, and optionally all subdirectories and their contents
// as well.
// If there are files existing under to_path, always overwrite.
// Returns true if successful, false otherwise.
// Don't use wildcards on the names, it may stop working without notice.
//
// If you only need to copy a file use CopyFile, it's faster.
BASE_EXPORT bool CopyDirectory(const FilePath& from_path,
                               const FilePath& to_path,
                               bool recursive);

// Returns true if the given path exists on the local filesystem,
// false otherwise.
BASE_EXPORT bool PathExists(const FilePath& path);

// Returns true if the given path is writable by the user, false otherwise.
BASE_EXPORT bool PathIsWritable(const FilePath& path);

// Returns true if the given path exists and is a directory, false otherwise.
BASE_EXPORT bool DirectoryExists(const FilePath& path);

#if defined(OS_WIN)
// Gets the creation time of the given file (expressed in the local timezone),
// and returns it via the creation_time parameter.  Returns true if successful,
// false otherwise.
BASE_EXPORT bool GetFileCreationLocalTime(const std::wstring& filename,
                                          LPSYSTEMTIME creation_time);

// Same as above, but takes a previously-opened file handle instead of a name.
BASE_EXPORT bool GetFileCreationLocalTimeFromHandle(HANDLE file_handle,
                                                    LPSYSTEMTIME creation_time);
#endif  // defined(OS_WIN)

// Returns true if the contents of the two files given are equal, false
// otherwise.  If either file can't be read, returns false.
BASE_EXPORT bool ContentsEqual(const FilePath& filename1,
                               const FilePath& filename2);

// Returns true if the contents of the two text files given are equal, false
// otherwise.  This routine treats "\r\n" and "\n" as equivalent.
BASE_EXPORT bool TextContentsEqual(const FilePath& filename1,
                                   const FilePath& filename2);

// Read the file at |path| into |contents|, returning true on success.
// |contents| may be NULL, in which case this function is useful for its
// side effect of priming the disk cache.
// Useful for unit tests.
BASE_EXPORT bool ReadFileToString(const FilePath& path, std::string* contents);

#if defined(OS_POSIX)
// Read exactly |bytes| bytes from file descriptor |fd|, storing the result
// in |buffer|. This function is protected against EINTR and partial reads.
// Returns true iff |bytes| bytes have been successfuly read from |fd|.
BASE_EXPORT bool ReadFromFD(int fd, char* buffer, size_t bytes);

// Creates a symbolic link at |symlink| pointing to |target|.  Returns
// false on failure.
BASE_EXPORT bool CreateSymbolicLink(const FilePath& target,
                                    const FilePath& symlink);

// Reads the given |symlink| and returns where it points to in |target|.
// Returns false upon failure.
BASE_EXPORT bool ReadSymbolicLink(const FilePath& symlink, FilePath* target);
#endif  // defined(OS_POSIX)

#if defined(OS_WIN)
// Resolve Windows shortcut (.LNK file)
// This methods tries to resolve a shortcut .LNK file. If the |path| is valid
// returns true and puts the target into the |path|, otherwise returns
// false leaving the path as it is.
BASE_EXPORT bool ResolveShortcut(FilePath* path);

// Create a Windows shortcut (.LNK file)
// This method creates a shortcut link using the information given. Ensure
// you have initialized COM before calling into this function. 'source'
// and 'destination' parameters are required, everything else can be NULL.
// 'source' is the existing file, 'destination' is the new link file to be
// created; for best results pass the filename with the .lnk extension.
// The 'icon' can specify a dll or exe in which case the icon index is the
// resource id. 'app_id' is the app model id for the shortcut on Win7.
// Note that if the shortcut exists it will overwrite it.
BASE_EXPORT bool CreateShortcutLink(const wchar_t *source,
                                    const wchar_t *destination,
                                    const wchar_t *working_dir,
                                    const wchar_t *arguments,
                                    const wchar_t *description,
                                    const wchar_t *icon,
                                    int icon_index,
                                    const wchar_t* app_id);

// Update a Windows shortcut (.LNK file). This method assumes the shortcut
// link already exists (otherwise false is returned). Ensure you have
// initialized COM before calling into this function. Only 'destination'
// parameter is required, everything else can be NULL (but if everything else
// is NULL no changes are made to the shortcut). 'destination' is the link
// file to be updated. 'app_id' is the app model id for the shortcut on Win7.
// For best results pass the filename with the .lnk extension.
BASE_EXPORT bool UpdateShortcutLink(const wchar_t *source,
                                    const wchar_t *destination,
                                    const wchar_t *working_dir,
                                    const wchar_t *arguments,
                                    const wchar_t *description,
                                    const wchar_t *icon,
                                    int icon_index,
                                    const wchar_t* app_id);

// Pins a shortcut to the Windows 7 taskbar. The shortcut file must already
// exist and be a shortcut that points to an executable.
BASE_EXPORT bool TaskbarPinShortcutLink(const wchar_t* shortcut);

// Unpins a shortcut from the Windows 7 taskbar. The shortcut must exist and
// already be pinned to the taskbar.
BASE_EXPORT bool TaskbarUnpinShortcutLink(const wchar_t* shortcut);

// Copy from_path to to_path recursively and then delete from_path recursively.
// Returns true if all operations succeed.
// This function simulates Move(), but unlike Move() it works across volumes.
// This fuction is not transactional.
BASE_EXPORT bool CopyAndDeleteDirectory(const FilePath& from_path,
                                        const FilePath& to_path);
#endif  // defined(OS_WIN)

// Return true if the given directory is empty
BASE_EXPORT bool IsDirectoryEmpty(const FilePath& dir_path);

// Get the temporary directory provided by the system.
// WARNING: DON'T USE THIS. If you want to create a temporary file, use one of
// the functions below.
BASE_EXPORT bool GetTempDir(FilePath* path);
// Get a temporary directory for shared memory files.
// Only useful on POSIX; redirects to GetTempDir() on Windows.
BASE_EXPORT bool GetShmemTempDir(FilePath* path, bool executable);

// Get the home directory.  This is more complicated than just getenv("HOME")
// as it knows to fall back on getpwent() etc.
BASE_EXPORT FilePath GetHomeDir();

// Creates a temporary file. The full path is placed in |path|, and the
// function returns true if was successful in creating the file. The file will
// be empty and all handles closed after this function returns.
BASE_EXPORT bool CreateTemporaryFile(FilePath* path);

// Same as CreateTemporaryFile but the file is created in |dir|.
BASE_EXPORT bool CreateTemporaryFileInDir(const FilePath& dir,
                                          FilePath* temp_file);

// Create and open a temporary file.  File is opened for read/write.
// The full path is placed in |path|.
// Returns a handle to the opened file or NULL if an error occured.
BASE_EXPORT FILE* CreateAndOpenTemporaryFile(FilePath* path);
// Like above but for shmem files.  Only useful for POSIX.
// The executable flag says the file needs to support using
// mprotect with PROT_EXEC after mapping.
BASE_EXPORT FILE* CreateAndOpenTemporaryShmemFile(FilePath* path,
                                                  bool executable);
// Similar to CreateAndOpenTemporaryFile, but the file is created in |dir|.
BASE_EXPORT FILE* CreateAndOpenTemporaryFileInDir(const FilePath& dir,
                                                  FilePath* path);

// Create a new directory. If prefix is provided, the new directory name is in
// the format of prefixyyyy.
// NOTE: prefix is ignored in the POSIX implementation.
// If success, return true and output the full path of the directory created.
BASE_EXPORT bool CreateNewTempDirectory(const FilePath::StringType& prefix,
                                        FilePath* new_temp_path);

// Create a directory within another directory.
// Extra characters will be appended to |prefix| to ensure that the
// new directory does not have the same name as an existing directory.
BASE_EXPORT bool CreateTemporaryDirInDir(const FilePath& base_dir,
                                         const FilePath::StringType& prefix,
                                         FilePath* new_dir);

// Creates a directory, as well as creating any parent directories, if they
// don't exist. Returns 'true' on successful creation, or if the directory
// already exists.  The directory is only readable by the current user.
BASE_EXPORT bool CreateDirectory(const FilePath& full_path);

// Returns the file size. Returns true on success.
BASE_EXPORT bool GetFileSize(const FilePath& file_path, int64* file_size);

// Returns true if the given path's base name is ".".
BASE_EXPORT bool IsDot(const FilePath& path);

// Returns true if the given path's base name is "..".
BASE_EXPORT bool IsDotDot(const FilePath& path);

// Sets |real_path| to |path| with symbolic links and junctions expanded.
// On windows, make sure the path starts with a lettered drive.
// |path| must reference a file.  Function will fail if |path| points to
// a directory or to a nonexistent path.  On windows, this function will
// fail if |path| is a junction or symlink that points to an empty file,
// or if |real_path| would be longer than MAX_PATH characters.
BASE_EXPORT bool NormalizeFilePath(const FilePath& path, FilePath* real_path);

#if defined(OS_WIN)

// Given a path in NT native form ("\Device\HarddiskVolumeXX\..."),
// return in |drive_letter_path| the equivalent path that starts with
// a drive letter ("C:\...").  Return false if no such path exists.
BASE_EXPORT bool DevicePathToDriveLetterPath(const FilePath& device_path,
                                             FilePath* drive_letter_path);

// Given an existing file in |path|, set |real_path| to the path
// in native NT format, of the form "\Device\HarddiskVolumeXX\..".
// Returns false if the path can not be found. Empty files cannot
// be resolved with this function.
BASE_EXPORT bool NormalizeToNativeFilePath(const FilePath& path,
                                           FilePath* nt_path);
#endif

// This function will return if the given file is a symlink or not.
BASE_EXPORT bool IsLink(const FilePath& file_path);

// Returns information about the given file path.
BASE_EXPORT bool GetFileInfo(const FilePath& file_path,
                             base::PlatformFileInfo* info);

// Sets the time of the last access and the time of the last modification.
BASE_EXPORT bool TouchFile(const FilePath& path,
                           const base::Time& last_accessed,
                           const base::Time& last_modified);

// Set the time of the last modification. Useful for unit tests.
BASE_EXPORT bool SetLastModifiedTime(const FilePath& path,
                                     const base::Time& last_modified);

#if defined(OS_POSIX)
// Store inode number of |path| in |inode|. Return true on success.
BASE_EXPORT bool GetInode(const FilePath& path, ino_t* inode);
#endif

// Wrapper for fopen-like calls. Returns non-NULL FILE* on success.
BASE_EXPORT FILE* OpenFile(const FilePath& filename, const char* mode);

// Closes file opened by OpenFile. Returns true on success.
BASE_EXPORT bool CloseFile(FILE* file);

// Truncates an open file to end at the location of the current file pointer.
// This is a cross-platform analog to Windows' SetEndOfFile() function.
BASE_EXPORT bool TruncateFile(FILE* file);

// Reads the given number of bytes from the file into the buffer.  Returns
// the number of read bytes, or -1 on error.
BASE_EXPORT int ReadFile(const FilePath& filename, char* data, int size);

// Writes the given buffer into the file, overwriting any data that was
// previously there.  Returns the number of bytes written, or -1 on error.
BASE_EXPORT int WriteFile(const FilePath& filename, const char* data, int size);
#if defined(OS_POSIX)
// Append the data to |fd|. Does not close |fd| when done.
BASE_EXPORT int WriteFileDescriptor(const int fd, const char* data, int size);
#endif

// Gets the current working directory for the process.
BASE_EXPORT bool GetCurrentDirectory(FilePath* path);

// Sets the current working directory for the process.
BASE_EXPORT bool SetCurrentDirectory(const FilePath& path);

#if defined(OS_POSIX)
// Test that |path| can only be changed by a given user and members of
// a given set of groups.
// Specifically, test that all parts of |path| under (and including) |base|:
// * Exist.
// * Are owned by a specific user.
// * Are not writable by all users.
// * Are owned by a memeber of a given set of groups, or are not writable by
//   their group.
// * Are not symbolic links.
// This is useful for checking that a config file is administrator-controlled.
// |base| must contain |path|.
BASE_EXPORT bool VerifyPathControlledByUser(const FilePath& base,
                                            const FilePath& path,
                                            uid_t owner_uid,
                                            const std::set<gid_t>& group_gids);
#endif  // defined(OS_POSIX)

#if defined(OS_MACOSX)
// Is |path| writable only by a user with administrator privileges?
// This function uses Mac OS conventions.  The super user is assumed to have
// uid 0, and the administrator group is assumed to be named "admin".
// Testing that |path|, and every parent directory including the root of
// the filesystem, are owned by the superuser, controlled by the group
// "admin", are not writable by all users, and contain no symbolic links.
// Will return false if |path| does not exist.
BASE_EXPORT bool VerifyPathControlledByAdmin(const FilePath& path);
#endif  // defined(OS_MACOSX)

// A class to handle auto-closing of FILE*'s.
class ScopedFILEClose {
 public:
  inline void operator()(FILE* x) const {
    if (x) {
      fclose(x);
    }
  }
};

typedef scoped_ptr_malloc<FILE, ScopedFILEClose> ScopedFILE;

#if defined(OS_POSIX)
// A class to handle auto-closing of FDs.
class ScopedFDClose {
 public:
  inline void operator()(int* x) const {
    if (x && *x >= 0) {
      if (HANDLE_EINTR(close(*x)) < 0)
        DPLOG(ERROR) << "close";
    }
  }
};

typedef scoped_ptr_malloc<int, ScopedFDClose> ScopedFD;
#endif  // OS_POSIX

// A class for enumerating the files in a provided path. The order of the
// results is not guaranteed.
//
// DO NOT USE FROM THE MAIN THREAD of your application unless it is a test
// program where latency does not matter. This class is blocking.
class BASE_EXPORT FileEnumerator {
 public:
#if defined(OS_WIN)
  typedef WIN32_FIND_DATA FindInfo;
#elif defined(OS_POSIX)
  typedef struct {
    struct stat stat;
    std::string filename;
  } FindInfo;
#endif

  enum FileType {
    FILES                 = 1 << 0,
    DIRECTORIES           = 1 << 1,
    INCLUDE_DOT_DOT       = 1 << 2,
#if defined(OS_POSIX)
    SHOW_SYM_LINKS        = 1 << 4,
#endif
  };

  // |root_path| is the starting directory to search for. It may or may not end
  // in a slash.
  //
  // If |recursive| is true, this will enumerate all matches in any
  // subdirectories matched as well. It does a breadth-first search, so all
  // files in one directory will be returned before any files in a
  // subdirectory.
  //
  // |file_type| specifies whether the enumerator should match files,
  // directories, or both.
  //
  // |pattern| is an optional pattern for which files to match. This
  // works like shell globbing. For example, "*.txt" or "Foo???.doc".
  // However, be careful in specifying patterns that aren't cross platform
  // since the underlying code uses OS-specific matching routines.  In general,
  // Windows matching is less featureful than others, so test there first.
  // If unspecified, this will match all files.
  // NOTE: the pattern only matches the contents of root_path, not files in
  // recursive subdirectories.
  // TODO(erikkay): Fix the pattern matching to work at all levels.
  FileEnumerator(const FilePath& root_path,
                 bool recursive,
                 FileType file_type);
  FileEnumerator(const FilePath& root_path,
                 bool recursive,
                 FileType file_type,
                 const FilePath::StringType& pattern);
  ~FileEnumerator();

  // Returns an empty string if there are no more results.
  FilePath Next();

  // Write the file info into |info|.
  void GetFindInfo(FindInfo* info);

  // Looks inside a FindInfo and determines if it's a directory.
  static bool IsDirectory(const FindInfo& info);

  static FilePath GetFilename(const FindInfo& find_info);
  static int64 GetFilesize(const FindInfo& find_info);
  static base::Time GetLastModifiedTime(const FindInfo& find_info);

 private:
  // Returns true if the given path should be skipped in enumeration.
  bool ShouldSkip(const FilePath& path);


#if defined(OS_WIN)
  // True when find_data_ is valid.
  bool has_find_data_;
  WIN32_FIND_DATA find_data_;
  HANDLE find_handle_;
#elif defined(OS_POSIX)
  struct DirectoryEntryInfo {
    FilePath filename;
    struct stat stat;
  };

  // Read the filenames in source into the vector of DirectoryEntryInfo's
  static bool ReadDirectory(std::vector<DirectoryEntryInfo>* entries,
                            const FilePath& source, bool show_links);

  // The files in the current directory
  std::vector<DirectoryEntryInfo> directory_entries_;

  // The next entry to use from the directory_entries_ vector
  size_t current_directory_entry_;
#endif

  FilePath root_path_;
  bool recursive_;
  FileType file_type_;
  FilePath::StringType pattern_;  // Empty when we want to find everything.

  // A stack that keeps track of which subdirectories we still need to
  // enumerate in the breadth-first search.
  std::stack<FilePath> pending_paths_;

  DISALLOW_COPY_AND_ASSIGN(FileEnumerator);
};

class BASE_EXPORT MemoryMappedFile {
 public:
  // The default constructor sets all members to invalid/null values.
  MemoryMappedFile();
  ~MemoryMappedFile();

  // Opens an existing file and maps it into memory. Access is restricted to
  // read only. If this object already points to a valid memory mapped file
  // then this method will fail and return false. If it cannot open the file,
  // the file does not exist, or the memory mapping fails, it will return false.
  // Later we may want to allow the user to specify access.
  bool Initialize(const FilePath& file_name);
  // As above, but works with an already-opened file. MemoryMappedFile will take
  // ownership of |file| and close it when done.
  bool Initialize(base::PlatformFile file);

#if defined(OS_WIN)
  // Opens an existing file and maps it as an image section. Please refer to
  // the Initialize function above for additional information.
  bool InitializeAsImageSection(const FilePath& file_name);
#endif  // OS_WIN

  const uint8* data() const { return data_; }
  size_t length() const { return length_; }

  // Is file_ a valid file handle that points to an open, memory mapped file?
  bool IsValid() const;

 private:
  // Open the given file and pass it to MapFileToMemoryInternal().
  bool MapFileToMemory(const FilePath& file_name);

  // Map the file to memory, set data_ to that memory address. Return true on
  // success, false on any kind of failure. This is a helper for Initialize().
  bool MapFileToMemoryInternal();

  // Closes all open handles. Later we may want to make this public.
  void CloseHandles();

#if defined(OS_WIN)
  // MapFileToMemoryInternal calls this function. It provides the ability to
  // pass in flags which control the mapped section.
  bool MapFileToMemoryInternalEx(int flags);

  HANDLE file_mapping_;
#endif
  base::PlatformFile file_;
  uint8* data_;
  size_t length_;

  DISALLOW_COPY_AND_ASSIGN(MemoryMappedFile);
};

// Returns whether the file has been modified since a particular date.
BASE_EXPORT bool HasFileBeenModifiedSince(
    const FileEnumerator::FindInfo& find_info,
    const base::Time& cutoff_time);

#if defined(OS_WIN)
  // Loads the file passed in as an image section and touches pages to avoid
  // subsequent hard page faults during LoadLibrary. The size to be pre read
  // is passed in. If it is 0 then the whole file is paged in. The step size
  // which indicates the number of bytes to skip after every page touched is
  // also passed in.
bool BASE_EXPORT PreReadImage(const wchar_t* file_path, size_t size_to_read,
                              size_t step_size);
#endif  // OS_WIN

#if defined(OS_LINUX)
// Broad categories of file systems as returned by statfs() on Linux.
enum FileSystemType {
  FILE_SYSTEM_UNKNOWN,  // statfs failed.
  FILE_SYSTEM_0,        // statfs.f_type == 0 means unknown, may indicate AFS.
  FILE_SYSTEM_ORDINARY,       // on-disk filesystem like ext2
  FILE_SYSTEM_NFS,
  FILE_SYSTEM_SMB,
  FILE_SYSTEM_CODA,
  FILE_SYSTEM_MEMORY,         // in-memory file system
  FILE_SYSTEM_CGROUP,         // cgroup control.
  FILE_SYSTEM_OTHER,          // any other value.
  FILE_SYSTEM_TYPE_COUNT
};

// Attempts determine the FileSystemType for |path|.
// Returns false if |path| doesn't exist.
BASE_EXPORT bool GetFileSystemType(const FilePath& path, FileSystemType* type);
#endif

}  // namespace file_util

// Deprecated functions have been moved to this separate header file,
// which must be included last after all the above definitions.
#include "base/file_util_deprecated.h"

#endif  // BASE_FILE_UTIL_H_
