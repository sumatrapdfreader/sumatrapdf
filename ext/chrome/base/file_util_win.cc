// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"

#include <windows.h>
#include <propvarutil.h>
#include <psapi.h>
#include <shellapi.h>
#include <shlobj.h>
#include <time.h>

#include <limits>
#include <string>

#include "base/file_path.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/threading/thread_restrictions.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "base/win/pe_image.h"
#include "base/win/scoped_comptr.h"
#include "base/win/scoped_handle.h"
#include "base/win/win_util.h"
#include "base/win/windows_version.h"

namespace file_util {

namespace {

const DWORD kFileShareAll =
    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;

}  // namespace

bool AbsolutePath(FilePath* path) {
  base::ThreadRestrictions::AssertIOAllowed();
  wchar_t file_path_buf[MAX_PATH];
  if (!_wfullpath(file_path_buf, path->value().c_str(), MAX_PATH))
    return false;
  *path = FilePath(file_path_buf);
  return true;
}

int CountFilesCreatedAfter(const FilePath& path,
                           const base::Time& comparison_time) {
  base::ThreadRestrictions::AssertIOAllowed();

  int file_count = 0;
  FILETIME comparison_filetime(comparison_time.ToFileTime());

  WIN32_FIND_DATA find_file_data;
  // All files in given dir
  std::wstring filename_spec = path.Append(L"*").value();
  HANDLE find_handle = FindFirstFile(filename_spec.c_str(), &find_file_data);
  if (find_handle != INVALID_HANDLE_VALUE) {
    do {
      // Don't count current or parent directories.
      if ((wcscmp(find_file_data.cFileName, L"..") == 0) ||
          (wcscmp(find_file_data.cFileName, L".") == 0))
        continue;

      long result = CompareFileTime(&find_file_data.ftCreationTime,  // NOLINT
                                    &comparison_filetime);
      // File was created after or on comparison time
      if ((result == 1) || (result == 0))
        ++file_count;
    } while (FindNextFile(find_handle,  &find_file_data));
    FindClose(find_handle);
  }

  return file_count;
}

bool Delete(const FilePath& path, bool recursive) {
  base::ThreadRestrictions::AssertIOAllowed();

  if (path.value().length() >= MAX_PATH)
    return false;

  if (!recursive) {
    // If not recursing, then first check to see if |path| is a directory.
    // If it is, then remove it with RemoveDirectory.
    base::PlatformFileInfo file_info;
    if (GetFileInfo(path, &file_info) && file_info.is_directory)
      return RemoveDirectory(path.value().c_str()) != 0;

    // Otherwise, it's a file, wildcard or non-existant. Try DeleteFile first
    // because it should be faster. If DeleteFile fails, then we fall through
    // to SHFileOperation, which will do the right thing.
    if (DeleteFile(path.value().c_str()) != 0)
      return true;
  }

  // SHFILEOPSTRUCT wants the path to be terminated with two NULLs,
  // so we have to use wcscpy because wcscpy_s writes non-NULLs
  // into the rest of the buffer.
  wchar_t double_terminated_path[MAX_PATH + 1] = {0};
#pragma warning(suppress:4996)  // don't complain about wcscpy deprecation
  wcscpy(double_terminated_path, path.value().c_str());

  SHFILEOPSTRUCT file_operation = {0};
  file_operation.wFunc = FO_DELETE;
  file_operation.pFrom = double_terminated_path;
  file_operation.fFlags = FOF_NOERRORUI | FOF_SILENT | FOF_NOCONFIRMATION;
  if (!recursive)
    file_operation.fFlags |= FOF_NORECURSION | FOF_FILESONLY;
  int err = SHFileOperation(&file_operation);

  // Since we're passing flags to the operation telling it to be silent,
  // it's possible for the operation to be aborted/cancelled without err
  // being set (although MSDN doesn't give any scenarios for how this can
  // happen).  See MSDN for SHFileOperation and SHFILEOPTSTRUCT.
  if (file_operation.fAnyOperationsAborted)
    return false;

  // Some versions of Windows return ERROR_FILE_NOT_FOUND (0x2) when deleting
  // an empty directory and some return 0x402 when they should be returning
  // ERROR_FILE_NOT_FOUND. MSDN says Vista and up won't return 0x402.
  return (err == 0 || err == ERROR_FILE_NOT_FOUND || err == 0x402);
}

bool DeleteAfterReboot(const FilePath& path) {
  base::ThreadRestrictions::AssertIOAllowed();

  if (path.value().length() >= MAX_PATH)
    return false;

  return MoveFileEx(path.value().c_str(), NULL,
                    MOVEFILE_DELAY_UNTIL_REBOOT |
                        MOVEFILE_REPLACE_EXISTING) != FALSE;
}

bool Move(const FilePath& from_path, const FilePath& to_path) {
  base::ThreadRestrictions::AssertIOAllowed();

  // NOTE: I suspect we could support longer paths, but that would involve
  // analyzing all our usage of files.
  if (from_path.value().length() >= MAX_PATH ||
      to_path.value().length() >= MAX_PATH) {
    return false;
  }
  if (MoveFileEx(from_path.value().c_str(), to_path.value().c_str(),
                 MOVEFILE_COPY_ALLOWED | MOVEFILE_REPLACE_EXISTING) != 0)
    return true;

  // Keep the last error value from MoveFileEx around in case the below
  // fails.
  bool ret = false;
  DWORD last_error = ::GetLastError();

  if (DirectoryExists(from_path)) {
    // MoveFileEx fails if moving directory across volumes. We will simulate
    // the move by using Copy and Delete. Ideally we could check whether
    // from_path and to_path are indeed in different volumes.
    ret = CopyAndDeleteDirectory(from_path, to_path);
  }

  if (!ret) {
    // Leave a clue about what went wrong so that it can be (at least) picked
    // up by a PLOG entry.
    ::SetLastError(last_error);
  }

  return ret;
}

bool ReplaceFile(const FilePath& from_path, const FilePath& to_path) {
  base::ThreadRestrictions::AssertIOAllowed();

  // Make sure that the target file exists.
  HANDLE target_file = ::CreateFile(
      to_path.value().c_str(),
      0,
      FILE_SHARE_READ | FILE_SHARE_WRITE,
      NULL,
      CREATE_NEW,
      FILE_ATTRIBUTE_NORMAL,
      NULL);
  if (target_file != INVALID_HANDLE_VALUE)
    ::CloseHandle(target_file);
  // When writing to a network share, we may not be able to change the ACLs.
  // Ignore ACL errors then (REPLACEFILE_IGNORE_MERGE_ERRORS).
  return ::ReplaceFile(to_path.value().c_str(),
      from_path.value().c_str(), NULL,
      REPLACEFILE_IGNORE_MERGE_ERRORS, NULL, NULL) ? true : false;
}

bool CopyFile(const FilePath& from_path, const FilePath& to_path) {
  base::ThreadRestrictions::AssertIOAllowed();

  // NOTE: I suspect we could support longer paths, but that would involve
  // analyzing all our usage of files.
  if (from_path.value().length() >= MAX_PATH ||
      to_path.value().length() >= MAX_PATH) {
    return false;
  }
  return (::CopyFile(from_path.value().c_str(), to_path.value().c_str(),
                     false) != 0);
}

bool ShellCopy(const FilePath& from_path, const FilePath& to_path,
               bool recursive) {
  base::ThreadRestrictions::AssertIOAllowed();

  // NOTE: I suspect we could support longer paths, but that would involve
  // analyzing all our usage of files.
  if (from_path.value().length() >= MAX_PATH ||
      to_path.value().length() >= MAX_PATH) {
    return false;
  }

  // SHFILEOPSTRUCT wants the path to be terminated with two NULLs,
  // so we have to use wcscpy because wcscpy_s writes non-NULLs
  // into the rest of the buffer.
  wchar_t double_terminated_path_from[MAX_PATH + 1] = {0};
  wchar_t double_terminated_path_to[MAX_PATH + 1] = {0};
#pragma warning(suppress:4996)  // don't complain about wcscpy deprecation
  wcscpy(double_terminated_path_from, from_path.value().c_str());
#pragma warning(suppress:4996)  // don't complain about wcscpy deprecation
  wcscpy(double_terminated_path_to, to_path.value().c_str());

  SHFILEOPSTRUCT file_operation = {0};
  file_operation.wFunc = FO_COPY;
  file_operation.pFrom = double_terminated_path_from;
  file_operation.pTo = double_terminated_path_to;
  file_operation.fFlags = FOF_NOERRORUI | FOF_SILENT | FOF_NOCONFIRMATION |
                          FOF_NOCONFIRMMKDIR;
  if (!recursive)
    file_operation.fFlags |= FOF_NORECURSION | FOF_FILESONLY;

  return (SHFileOperation(&file_operation) == 0);
}

bool CopyDirectory(const FilePath& from_path, const FilePath& to_path,
                   bool recursive) {
  base::ThreadRestrictions::AssertIOAllowed();

  if (recursive)
    return ShellCopy(from_path, to_path, true);

  // The following code assumes that from path is a directory.
  DCHECK(DirectoryExists(from_path));

  // Instead of creating a new directory, we copy the old one to include the
  // security information of the folder as part of the copy.
  if (!PathExists(to_path)) {
    // Except that Vista fails to do that, and instead do a recursive copy if
    // the target directory doesn't exist.
    if (base::win::GetVersion() >= base::win::VERSION_VISTA)
      CreateDirectory(to_path);
    else
      ShellCopy(from_path, to_path, false);
  }

  FilePath directory = from_path.Append(L"*.*");
  return ShellCopy(directory, to_path, false);
}

bool CopyAndDeleteDirectory(const FilePath& from_path,
                            const FilePath& to_path) {
  base::ThreadRestrictions::AssertIOAllowed();
  if (CopyDirectory(from_path, to_path, true)) {
    if (Delete(from_path, true)) {
      return true;
    }
    // Like Move, this function is not transactional, so we just
    // leave the copied bits behind if deleting from_path fails.
    // If to_path exists previously then we have already overwritten
    // it by now, we don't get better off by deleting the new bits.
  }
  return false;
}


bool PathExists(const FilePath& path) {
  base::ThreadRestrictions::AssertIOAllowed();
  return (GetFileAttributes(path.value().c_str()) != INVALID_FILE_ATTRIBUTES);
}

bool PathIsWritable(const FilePath& path) {
  base::ThreadRestrictions::AssertIOAllowed();
  HANDLE dir =
      CreateFile(path.value().c_str(), FILE_ADD_FILE, kFileShareAll,
                 NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);

  if (dir == INVALID_HANDLE_VALUE)
    return false;

  CloseHandle(dir);
  return true;
}

bool DirectoryExists(const FilePath& path) {
  base::ThreadRestrictions::AssertIOAllowed();
  DWORD fileattr = GetFileAttributes(path.value().c_str());
  if (fileattr != INVALID_FILE_ATTRIBUTES)
    return (fileattr & FILE_ATTRIBUTE_DIRECTORY) != 0;
  return false;
}

bool GetFileCreationLocalTimeFromHandle(HANDLE file_handle,
                                        LPSYSTEMTIME creation_time) {
  base::ThreadRestrictions::AssertIOAllowed();
  if (!file_handle)
    return false;

  FILETIME utc_filetime;
  if (!GetFileTime(file_handle, &utc_filetime, NULL, NULL))
    return false;

  FILETIME local_filetime;
  if (!FileTimeToLocalFileTime(&utc_filetime, &local_filetime))
    return false;

  return !!FileTimeToSystemTime(&local_filetime, creation_time);
}

bool GetFileCreationLocalTime(const std::wstring& filename,
                              LPSYSTEMTIME creation_time) {
  base::ThreadRestrictions::AssertIOAllowed();
  base::win::ScopedHandle file_handle(
      CreateFile(filename.c_str(), GENERIC_READ, kFileShareAll, NULL,
                 OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL));
  return GetFileCreationLocalTimeFromHandle(file_handle.Get(), creation_time);
}

bool ResolveShortcut(FilePath* path) {
  base::ThreadRestrictions::AssertIOAllowed();

  HRESULT result;
  base::win::ScopedComPtr<IShellLink> i_shell_link;
  bool is_resolved = false;

  // Get pointer to the IShellLink interface
  result = i_shell_link.CreateInstance(CLSID_ShellLink, NULL,
                                       CLSCTX_INPROC_SERVER);
  if (SUCCEEDED(result)) {
    base::win::ScopedComPtr<IPersistFile> persist;
    // Query IShellLink for the IPersistFile interface
    result = persist.QueryFrom(i_shell_link);
    if (SUCCEEDED(result)) {
      WCHAR temp_path[MAX_PATH];
      // Load the shell link
      result = persist->Load(path->value().c_str(), STGM_READ);
      if (SUCCEEDED(result)) {
        // Try to find the target of a shortcut
        result = i_shell_link->Resolve(0, SLR_NO_UI);
        if (SUCCEEDED(result)) {
          result = i_shell_link->GetPath(temp_path, MAX_PATH,
                                  NULL, SLGP_UNCPRIORITY);
          *path = FilePath(temp_path);
          is_resolved = true;
        }
      }
    }
  }

  return is_resolved;
}

bool CreateShortcutLink(const wchar_t *source, const wchar_t *destination,
                        const wchar_t *working_dir, const wchar_t *arguments,
                        const wchar_t *description, const wchar_t *icon,
                        int icon_index, const wchar_t* app_id) {
  base::ThreadRestrictions::AssertIOAllowed();

  // Length of arguments and description must be less than MAX_PATH.
  DCHECK(lstrlen(arguments) < MAX_PATH);
  DCHECK(lstrlen(description) < MAX_PATH);

  base::win::ScopedComPtr<IShellLink> i_shell_link;
  base::win::ScopedComPtr<IPersistFile> i_persist_file;

  // Get pointer to the IShellLink interface
  HRESULT result = i_shell_link.CreateInstance(CLSID_ShellLink, NULL,
                                               CLSCTX_INPROC_SERVER);
  if (FAILED(result))
    return false;

  // Query IShellLink for the IPersistFile interface
  result = i_persist_file.QueryFrom(i_shell_link);
  if (FAILED(result))
    return false;

  if (FAILED(i_shell_link->SetPath(source)))
    return false;

  if (working_dir && FAILED(i_shell_link->SetWorkingDirectory(working_dir)))
    return false;

  if (arguments && FAILED(i_shell_link->SetArguments(arguments)))
    return false;

  if (description && FAILED(i_shell_link->SetDescription(description)))
    return false;

  if (icon && FAILED(i_shell_link->SetIconLocation(icon, icon_index)))
    return false;

  if (app_id && (base::win::GetVersion() >= base::win::VERSION_WIN7)) {
    base::win::ScopedComPtr<IPropertyStore> property_store;
    if (FAILED(property_store.QueryFrom(i_shell_link)))
      return false;

    if (!base::win::SetAppIdForPropertyStore(property_store, app_id))
      return false;
  }

  result = i_persist_file->Save(destination, TRUE);
  return SUCCEEDED(result);
}

bool UpdateShortcutLink(const wchar_t *source, const wchar_t *destination,
                        const wchar_t *working_dir, const wchar_t *arguments,
                        const wchar_t *description, const wchar_t *icon,
                        int icon_index, const wchar_t* app_id) {
  base::ThreadRestrictions::AssertIOAllowed();

  // Length of arguments and description must be less than MAX_PATH.
  DCHECK(lstrlen(arguments) < MAX_PATH);
  DCHECK(lstrlen(description) < MAX_PATH);

  // Get pointer to the IPersistFile interface and load existing link
  base::win::ScopedComPtr<IShellLink> i_shell_link;
  if (FAILED(i_shell_link.CreateInstance(CLSID_ShellLink, NULL,
                                         CLSCTX_INPROC_SERVER)))
    return false;

  base::win::ScopedComPtr<IPersistFile> i_persist_file;
  if (FAILED(i_persist_file.QueryFrom(i_shell_link)))
    return false;

  if (FAILED(i_persist_file->Load(destination, STGM_READWRITE)))
    return false;

  if (source && FAILED(i_shell_link->SetPath(source)))
    return false;

  if (working_dir && FAILED(i_shell_link->SetWorkingDirectory(working_dir)))
    return false;

  if (arguments && FAILED(i_shell_link->SetArguments(arguments)))
    return false;

  if (description && FAILED(i_shell_link->SetDescription(description)))
    return false;

  if (icon && FAILED(i_shell_link->SetIconLocation(icon, icon_index)))
    return false;

  if (app_id && base::win::GetVersion() >= base::win::VERSION_WIN7) {
    base::win::ScopedComPtr<IPropertyStore> property_store;
    if (FAILED(property_store.QueryFrom(i_shell_link)))
      return false;

    if (!base::win::SetAppIdForPropertyStore(property_store, app_id))
      return false;
  }

  HRESULT result = i_persist_file->Save(destination, TRUE);

  i_persist_file.Release();
  i_shell_link.Release();

  // If we successfully updated the icon, notify the shell that we have done so.
  if (SUCCEEDED(result)) {
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST | SHCNF_FLUSHNOWAIT,
                   NULL, NULL);
  }

  return SUCCEEDED(result);
}

bool TaskbarPinShortcutLink(const wchar_t* shortcut) {
  base::ThreadRestrictions::AssertIOAllowed();

  // "Pin to taskbar" is only supported after Win7.
  if (base::win::GetVersion() < base::win::VERSION_WIN7)
    return false;

  int result = reinterpret_cast<int>(ShellExecute(NULL, L"taskbarpin", shortcut,
      NULL, NULL, 0));
  return result > 32;
}

bool TaskbarUnpinShortcutLink(const wchar_t* shortcut) {
  base::ThreadRestrictions::AssertIOAllowed();

  // "Unpin from taskbar" is only supported after Win7.
  if (base::win::GetVersion() < base::win::VERSION_WIN7)
    return false;

  int result = reinterpret_cast<int>(ShellExecute(NULL, L"taskbarunpin",
      shortcut, NULL, NULL, 0));
  return result > 32;
}

bool GetTempDir(FilePath* path) {
  base::ThreadRestrictions::AssertIOAllowed();

  wchar_t temp_path[MAX_PATH + 1];
  DWORD path_len = ::GetTempPath(MAX_PATH, temp_path);
  if (path_len >= MAX_PATH || path_len <= 0)
    return false;
  // TODO(evanm): the old behavior of this function was to always strip the
  // trailing slash.  We duplicate this here, but it shouldn't be necessary
  // when everyone is using the appropriate FilePath APIs.
  *path = FilePath(temp_path).StripTrailingSeparators();
  return true;
}

bool GetShmemTempDir(FilePath* path, bool executable) {
  return GetTempDir(path);
}

bool CreateTemporaryFile(FilePath* path) {
  base::ThreadRestrictions::AssertIOAllowed();

  FilePath temp_file;

  if (!GetTempDir(path))
    return false;

  if (CreateTemporaryFileInDir(*path, &temp_file)) {
    *path = temp_file;
    return true;
  }

  return false;
}

FILE* CreateAndOpenTemporaryShmemFile(FilePath* path, bool executable) {
  base::ThreadRestrictions::AssertIOAllowed();
  return CreateAndOpenTemporaryFile(path);
}

// On POSIX we have semantics to create and open a temporary file
// atomically.
// TODO(jrg): is there equivalent call to use on Windows instead of
// going 2-step?
FILE* CreateAndOpenTemporaryFileInDir(const FilePath& dir, FilePath* path) {
  base::ThreadRestrictions::AssertIOAllowed();
  if (!CreateTemporaryFileInDir(dir, path)) {
    return NULL;
  }
  // Open file in binary mode, to avoid problems with fwrite. On Windows
  // it replaces \n's with \r\n's, which may surprise you.
  // Reference: http://msdn.microsoft.com/en-us/library/h9t88zwz(VS.71).aspx
  return OpenFile(*path, "wb+");
}

bool CreateTemporaryFileInDir(const FilePath& dir,
                              FilePath* temp_file) {
  base::ThreadRestrictions::AssertIOAllowed();

  wchar_t temp_name[MAX_PATH + 1];

  if (!GetTempFileName(dir.value().c_str(), L"", 0, temp_name)) {
    DPLOG(WARNING) << "Failed to get temporary file name in " << dir.value();
    return false;
  }

  DWORD path_len = GetLongPathName(temp_name, temp_name, MAX_PATH);
  if (path_len > MAX_PATH + 1 || path_len == 0) {
    DPLOG(WARNING) << "Failed to get long path name for " << temp_name;
    return false;
  }

  std::wstring temp_file_str;
  temp_file_str.assign(temp_name, path_len);
  *temp_file = FilePath(temp_file_str);
  return true;
}

bool CreateTemporaryDirInDir(const FilePath& base_dir,
                             const FilePath::StringType& prefix,
                             FilePath* new_dir) {
  base::ThreadRestrictions::AssertIOAllowed();

  FilePath path_to_create;
  srand(static_cast<uint32>(time(NULL)));

  for (int count = 0; count < 50; ++count) {
    // Try create a new temporary directory with random generated name. If
    // the one exists, keep trying another path name until we reach some limit.
    string16 new_dir_name;
    new_dir_name.assign(prefix);
    new_dir_name.append(base::IntToString16(rand() % kint16max));

    path_to_create = base_dir.Append(new_dir_name);
    if (::CreateDirectory(path_to_create.value().c_str(), NULL)) {
      *new_dir = path_to_create;
      return true;
    }
  }

  return false;
}

bool CreateNewTempDirectory(const FilePath::StringType& prefix,
                            FilePath* new_temp_path) {
  base::ThreadRestrictions::AssertIOAllowed();

  FilePath system_temp_dir;
  if (!GetTempDir(&system_temp_dir))
    return false;

  return CreateTemporaryDirInDir(system_temp_dir, prefix, new_temp_path);
}

bool CreateDirectory(const FilePath& full_path) {
  base::ThreadRestrictions::AssertIOAllowed();

  // If the path exists, we've succeeded if it's a directory, failed otherwise.
  const wchar_t* full_path_str = full_path.value().c_str();
  DWORD fileattr = ::GetFileAttributes(full_path_str);
  if (fileattr != INVALID_FILE_ATTRIBUTES) {
    if ((fileattr & FILE_ATTRIBUTE_DIRECTORY) != 0) {
      DVLOG(1) << "CreateDirectory(" << full_path_str << "), "
               << "directory already exists.";
      return true;
    }
    DLOG(WARNING) << "CreateDirectory(" << full_path_str << "), "
                  << "conflicts with existing file.";
    return false;
  }

  // Invariant:  Path does not exist as file or directory.

  // Attempt to create the parent recursively.  This will immediately return
  // true if it already exists, otherwise will create all required parent
  // directories starting with the highest-level missing parent.
  FilePath parent_path(full_path.DirName());
  if (parent_path.value() == full_path.value()) {
    return false;
  }
  if (!CreateDirectory(parent_path)) {
    DLOG(WARNING) << "Failed to create one of the parent directories.";
    return false;
  }

  if (!::CreateDirectory(full_path_str, NULL)) {
    DWORD error_code = ::GetLastError();
    if (error_code == ERROR_ALREADY_EXISTS && DirectoryExists(full_path)) {
      // This error code ERROR_ALREADY_EXISTS doesn't indicate whether we
      // were racing with someone creating the same directory, or a file
      // with the same path.  If DirectoryExists() returns true, we lost the
      // race to create the same directory.
      return true;
    } else {
      DLOG(WARNING) << "Failed to create directory " << full_path_str
                    << ", last error is " << error_code << ".";
      return false;
    }
  } else {
    return true;
  }
}

// TODO(rkc): Work out if we want to handle NTFS junctions here or not, handle
// them if we do decide to.
bool IsLink(const FilePath& file_path) {
  return false;
}

bool GetFileInfo(const FilePath& file_path, base::PlatformFileInfo* results) {
  base::ThreadRestrictions::AssertIOAllowed();

  WIN32_FILE_ATTRIBUTE_DATA attr;
  if (!GetFileAttributesEx(file_path.value().c_str(),
                           GetFileExInfoStandard, &attr)) {
    return false;
  }

  ULARGE_INTEGER size;
  size.HighPart = attr.nFileSizeHigh;
  size.LowPart = attr.nFileSizeLow;
  results->size = size.QuadPart;

  results->is_directory =
      (attr.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
  results->last_modified = base::Time::FromFileTime(attr.ftLastWriteTime);
  results->last_accessed = base::Time::FromFileTime(attr.ftLastAccessTime);
  results->creation_time = base::Time::FromFileTime(attr.ftCreationTime);

  return true;
}

FILE* OpenFile(const FilePath& filename, const char* mode) {
  base::ThreadRestrictions::AssertIOAllowed();
  std::wstring w_mode = ASCIIToWide(std::string(mode));
  return _wfsopen(filename.value().c_str(), w_mode.c_str(), _SH_DENYNO);
}

FILE* OpenFile(const std::string& filename, const char* mode) {
  base::ThreadRestrictions::AssertIOAllowed();
  return _fsopen(filename.c_str(), mode, _SH_DENYNO);
}

int ReadFile(const FilePath& filename, char* data, int size) {
  base::ThreadRestrictions::AssertIOAllowed();
  base::win::ScopedHandle file(CreateFile(filename.value().c_str(),
                                          GENERIC_READ,
                                          FILE_SHARE_READ | FILE_SHARE_WRITE,
                                          NULL,
                                          OPEN_EXISTING,
                                          FILE_FLAG_SEQUENTIAL_SCAN,
                                          NULL));
  if (!file)
    return -1;

  DWORD read;
  if (::ReadFile(file, data, size, &read, NULL) &&
      static_cast<int>(read) == size)
    return read;
  return -1;
}

int WriteFile(const FilePath& filename, const char* data, int size) {
  base::ThreadRestrictions::AssertIOAllowed();
  base::win::ScopedHandle file(CreateFile(filename.value().c_str(),
                                          GENERIC_WRITE,
                                          0,
                                          NULL,
                                          CREATE_ALWAYS,
                                          0,
                                          NULL));
  if (!file) {
    DLOG(WARNING) << "CreateFile failed for path " << filename.value()
                  << " error code=" << GetLastError();
    return -1;
  }

  DWORD written;
  BOOL result = ::WriteFile(file, data, size, &written, NULL);
  if (result && static_cast<int>(written) == size)
    return written;

  if (!result) {
    // WriteFile failed.
    DLOG(WARNING) << "writing file " << filename.value()
                  << " failed, error code=" << GetLastError();
  } else {
    // Didn't write all the bytes.
    DLOG(WARNING) << "wrote" << written << " bytes to "
                  << filename.value() << " expected " << size;
  }
  return -1;
}

// Gets the current working directory for the process.
bool GetCurrentDirectory(FilePath* dir) {
  base::ThreadRestrictions::AssertIOAllowed();

  wchar_t system_buffer[MAX_PATH];
  system_buffer[0] = 0;
  DWORD len = ::GetCurrentDirectory(MAX_PATH, system_buffer);
  if (len == 0 || len > MAX_PATH)
    return false;
  // TODO(evanm): the old behavior of this function was to always strip the
  // trailing slash.  We duplicate this here, but it shouldn't be necessary
  // when everyone is using the appropriate FilePath APIs.
  std::wstring dir_str(system_buffer);
  *dir = FilePath(dir_str).StripTrailingSeparators();
  return true;
}

// Sets the current working directory for the process.
bool SetCurrentDirectory(const FilePath& directory) {
  base::ThreadRestrictions::AssertIOAllowed();
  BOOL ret = ::SetCurrentDirectory(directory.value().c_str());
  return ret != 0;
}

///////////////////////////////////////////////
// FileEnumerator

FileEnumerator::FileEnumerator(const FilePath& root_path,
                               bool recursive,
                               FileType file_type)
    : recursive_(recursive),
      file_type_(file_type),
      has_find_data_(false),
      find_handle_(INVALID_HANDLE_VALUE) {
  // INCLUDE_DOT_DOT must not be specified if recursive.
  DCHECK(!(recursive && (INCLUDE_DOT_DOT & file_type_)));
  memset(&find_data_, 0, sizeof(find_data_));
  pending_paths_.push(root_path);
}

FileEnumerator::FileEnumerator(const FilePath& root_path,
                               bool recursive,
                               FileType file_type,
                               const FilePath::StringType& pattern)
    : recursive_(recursive),
      file_type_(file_type),
      has_find_data_(false),
      pattern_(pattern),
      find_handle_(INVALID_HANDLE_VALUE) {
  // INCLUDE_DOT_DOT must not be specified if recursive.
  DCHECK(!(recursive && (INCLUDE_DOT_DOT & file_type_)));
  memset(&find_data_, 0, sizeof(find_data_));
  pending_paths_.push(root_path);
}

FileEnumerator::~FileEnumerator() {
  if (find_handle_ != INVALID_HANDLE_VALUE)
    FindClose(find_handle_);
}

void FileEnumerator::GetFindInfo(FindInfo* info) {
  DCHECK(info);

  if (!has_find_data_)
    return;

  memcpy(info, &find_data_, sizeof(*info));
}

bool FileEnumerator::IsDirectory(const FindInfo& info) {
  return (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

// static
FilePath FileEnumerator::GetFilename(const FindInfo& find_info) {
  return FilePath(find_info.cFileName);
}

// static
int64 FileEnumerator::GetFilesize(const FindInfo& find_info) {
  ULARGE_INTEGER size;
  size.HighPart = find_info.nFileSizeHigh;
  size.LowPart = find_info.nFileSizeLow;
  DCHECK_LE(size.QuadPart, std::numeric_limits<int64>::max());
  return static_cast<int64>(size.QuadPart);
}

// static
base::Time FileEnumerator::GetLastModifiedTime(const FindInfo& find_info) {
  return base::Time::FromFileTime(find_info.ftLastWriteTime);
}

FilePath FileEnumerator::Next() {
  base::ThreadRestrictions::AssertIOAllowed();

  while (has_find_data_ || !pending_paths_.empty()) {
    if (!has_find_data_) {
      // The last find FindFirstFile operation is done, prepare a new one.
      root_path_ = pending_paths_.top();
      pending_paths_.pop();

      // Start a new find operation.
      FilePath src = root_path_;

      if (pattern_.empty())
        src = src.Append(L"*");  // No pattern = match everything.
      else
        src = src.Append(pattern_);

      find_handle_ = FindFirstFile(src.value().c_str(), &find_data_);
      has_find_data_ = true;
    } else {
      // Search for the next file/directory.
      if (!FindNextFile(find_handle_, &find_data_)) {
        FindClose(find_handle_);
        find_handle_ = INVALID_HANDLE_VALUE;
      }
    }

    if (INVALID_HANDLE_VALUE == find_handle_) {
      has_find_data_ = false;

      // This is reached when we have finished a directory and are advancing to
      // the next one in the queue. We applied the pattern (if any) to the files
      // in the root search directory, but for those directories which were
      // matched, we want to enumerate all files inside them. This will happen
      // when the handle is empty.
      pattern_ = FilePath::StringType();

      continue;
    }

    FilePath cur_file(find_data_.cFileName);
    if (ShouldSkip(cur_file))
      continue;

    // Construct the absolute filename.
    cur_file = root_path_.Append(find_data_.cFileName);

    if (find_data_.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
      if (recursive_) {
        // If |cur_file| is a directory, and we are doing recursive searching,
        // add it to pending_paths_ so we scan it after we finish scanning this
        // directory.
        pending_paths_.push(cur_file);
      }
      if (file_type_ & FileEnumerator::DIRECTORIES)
        return cur_file;
    } else if (file_type_ & FileEnumerator::FILES) {
      return cur_file;
    }
  }

  return FilePath();
}

///////////////////////////////////////////////
// MemoryMappedFile

MemoryMappedFile::MemoryMappedFile()
    : file_(INVALID_HANDLE_VALUE),
      file_mapping_(INVALID_HANDLE_VALUE),
      data_(NULL),
      length_(INVALID_FILE_SIZE) {
}

bool MemoryMappedFile::InitializeAsImageSection(const FilePath& file_name) {
  if (IsValid())
    return false;
  file_ = base::CreatePlatformFile(
      file_name, base::PLATFORM_FILE_OPEN | base::PLATFORM_FILE_READ,
      NULL, NULL);

  if (file_ == base::kInvalidPlatformFileValue) {
    DLOG(ERROR) << "Couldn't open " << file_name.value();
    return false;
  }

  if (!MapFileToMemoryInternalEx(SEC_IMAGE)) {
    CloseHandles();
    return false;
  }

  return true;
}

bool MemoryMappedFile::MapFileToMemoryInternal() {
  return MapFileToMemoryInternalEx(0);
}

bool MemoryMappedFile::MapFileToMemoryInternalEx(int flags) {
  base::ThreadRestrictions::AssertIOAllowed();

  if (file_ == INVALID_HANDLE_VALUE)
    return false;

  length_ = ::GetFileSize(file_, NULL);
  if (length_ == INVALID_FILE_SIZE)
    return false;

  file_mapping_ = ::CreateFileMapping(file_, NULL, PAGE_READONLY | flags,
                                      0, 0, NULL);
  if (!file_mapping_) {
    // According to msdn, system error codes are only reserved up to 15999.
    // http://msdn.microsoft.com/en-us/library/ms681381(v=VS.85).aspx.
    UMA_HISTOGRAM_ENUMERATION("MemoryMappedFile.CreateFileMapping",
                              logging::GetLastSystemErrorCode(), 16000);
    return false;
  }

  data_ = static_cast<uint8*>(
      ::MapViewOfFile(file_mapping_, FILE_MAP_READ, 0, 0, 0));
  if (!data_) {
    UMA_HISTOGRAM_ENUMERATION("MemoryMappedFile.MapViewOfFile",
                              logging::GetLastSystemErrorCode(), 16000);
  }
  return data_ != NULL;
}

void MemoryMappedFile::CloseHandles() {
  if (data_)
    ::UnmapViewOfFile(data_);
  if (file_mapping_ != INVALID_HANDLE_VALUE)
    ::CloseHandle(file_mapping_);
  if (file_ != INVALID_HANDLE_VALUE)
    ::CloseHandle(file_);

  data_ = NULL;
  file_mapping_ = file_ = INVALID_HANDLE_VALUE;
  length_ = INVALID_FILE_SIZE;
}

bool HasFileBeenModifiedSince(const FileEnumerator::FindInfo& find_info,
                              const base::Time& cutoff_time) {
  base::ThreadRestrictions::AssertIOAllowed();
  FILETIME file_time = cutoff_time.ToFileTime();
  long result = CompareFileTime(&find_info.ftLastWriteTime,  // NOLINT
                                &file_time);
  return result == 1 || result == 0;
}

bool NormalizeFilePath(const FilePath& path, FilePath* real_path) {
  base::ThreadRestrictions::AssertIOAllowed();
  FilePath mapped_file;
  if (!NormalizeToNativeFilePath(path, &mapped_file))
    return false;
  // NormalizeToNativeFilePath() will return a path that starts with
  // "\Device\Harddisk...".  Helper DevicePathToDriveLetterPath()
  // will find a drive letter which maps to the path's device, so
  // that we return a path starting with a drive letter.
  return DevicePathToDriveLetterPath(mapped_file, real_path);
}

bool DevicePathToDriveLetterPath(const FilePath& nt_device_path,
                                 FilePath* out_drive_letter_path) {
  base::ThreadRestrictions::AssertIOAllowed();

  // Get the mapping of drive letters to device paths.
  const int kDriveMappingSize = 1024;
  wchar_t drive_mapping[kDriveMappingSize] = {'\0'};
  if (!::GetLogicalDriveStrings(kDriveMappingSize - 1, drive_mapping)) {
    DLOG(ERROR) << "Failed to get drive mapping.";
    return false;
  }

  // The drive mapping is a sequence of null terminated strings.
  // The last string is empty.
  wchar_t* drive_map_ptr = drive_mapping;
  wchar_t device_path_as_string[MAX_PATH];
  wchar_t drive[] = L" :";

  // For each string in the drive mapping, get the junction that links
  // to it.  If that junction is a prefix of |device_path|, then we
  // know that |drive| is the real path prefix.
  while (*drive_map_ptr) {
    drive[0] = drive_map_ptr[0];  // Copy the drive letter.

    if (QueryDosDevice(drive, device_path_as_string, MAX_PATH)) {
      FilePath device_path(device_path_as_string);
      if (device_path == nt_device_path ||
          device_path.IsParent(nt_device_path)) {
        *out_drive_letter_path = FilePath(drive +
            nt_device_path.value().substr(wcslen(device_path_as_string)));
        return true;
      }
    }
    // Move to the next drive letter string, which starts one
    // increment after the '\0' that terminates the current string.
    while (*drive_map_ptr++);
  }

  // No drive matched.  The path does not start with a device junction
  // that is mounted as a drive letter.  This means there is no drive
  // letter path to the volume that holds |device_path|, so fail.
  return false;
}

bool NormalizeToNativeFilePath(const FilePath& path, FilePath* nt_path) {
  base::ThreadRestrictions::AssertIOAllowed();
  // In Vista, GetFinalPathNameByHandle() would give us the real path
  // from a file handle.  If we ever deprecate XP, consider changing the
  // code below to a call to GetFinalPathNameByHandle().  The method this
  // function uses is explained in the following msdn article:
  // http://msdn.microsoft.com/en-us/library/aa366789(VS.85).aspx
  base::win::ScopedHandle file_handle(
      ::CreateFile(path.value().c_str(),
                   GENERIC_READ,
                   kFileShareAll,
                   NULL,
                   OPEN_EXISTING,
                   FILE_ATTRIBUTE_NORMAL,
                   NULL));
  if (!file_handle)
    return false;

  // Create a file mapping object.  Can't easily use MemoryMappedFile, because
  // we only map the first byte, and need direct access to the handle. You can
  // not map an empty file, this call fails in that case.
  base::win::ScopedHandle file_map_handle(
      ::CreateFileMapping(file_handle.Get(),
                          NULL,
                          PAGE_READONLY,
                          0,
                          1,  // Just one byte.  No need to look at the data.
                          NULL));
  if (!file_map_handle)
    return false;

  // Use a view of the file to get the path to the file.
  void* file_view = MapViewOfFile(file_map_handle.Get(),
                                  FILE_MAP_READ, 0, 0, 1);
  if (!file_view)
    return false;

  // The expansion of |path| into a full path may make it longer.
  // GetMappedFileName() will fail if the result is longer than MAX_PATH.
  // Pad a bit to be safe.  If kMaxPathLength is ever changed to be less
  // than MAX_PATH, it would be nessisary to test that GetMappedFileName()
  // not return kMaxPathLength.  This would mean that only part of the
  // path fit in |mapped_file_path|.
  const int kMaxPathLength = MAX_PATH + 10;
  wchar_t mapped_file_path[kMaxPathLength];
  bool success = false;
  HANDLE cp = GetCurrentProcess();
  if (::GetMappedFileNameW(cp, file_view, mapped_file_path, kMaxPathLength)) {
    *nt_path = FilePath(mapped_file_path);
    success = true;
  }
  ::UnmapViewOfFile(file_view);
  return success;
}

bool PreReadImage(const wchar_t* file_path, size_t size_to_read,
                  size_t step_size) {
  base::ThreadRestrictions::AssertIOAllowed();
  if (base::win::GetVersion() > base::win::VERSION_XP) {
    // Vista+ branch. On these OSes, the forced reads through the DLL actually
    // slows warm starts. The solution is to sequentially read file contents
    // with an optional cap on total amount to read.
    base::win::ScopedHandle file(
        CreateFile(file_path,
                   GENERIC_READ,
                   FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                   NULL,
                   OPEN_EXISTING,
                   FILE_FLAG_SEQUENTIAL_SCAN,
                   NULL));

    if (!file.IsValid())
      return false;

    // Default to 1MB sequential reads.
    const DWORD actual_step_size = std::max(static_cast<DWORD>(step_size),
                                            static_cast<DWORD>(1024*1024));
    LPVOID buffer = ::VirtualAlloc(NULL,
                                   actual_step_size,
                                   MEM_COMMIT,
                                   PAGE_READWRITE);

    if (buffer == NULL)
      return false;

    DWORD len;
    size_t total_read = 0;
    while (::ReadFile(file, buffer, actual_step_size, &len, NULL) &&
           len > 0 &&
           (size_to_read ? total_read < size_to_read : true)) {
      total_read += static_cast<size_t>(len);
    }
    ::VirtualFree(buffer, 0, MEM_RELEASE);
  } else {
    // WinXP branch. Here, reading the DLL from disk doesn't do
    // what we want so instead we pull the pages into memory by loading
    // the DLL and touching pages at a stride.
    HMODULE dll_module = ::LoadLibraryExW(
        file_path,
        NULL,
        LOAD_WITH_ALTERED_SEARCH_PATH | DONT_RESOLVE_DLL_REFERENCES);

    if (!dll_module)
      return false;

    base::win::PEImage pe_image(dll_module);
    PIMAGE_NT_HEADERS nt_headers = pe_image.GetNTHeaders();
    size_t actual_size_to_read = size_to_read ? size_to_read :
                                 nt_headers->OptionalHeader.SizeOfImage;
    volatile uint8* touch = reinterpret_cast<uint8*>(dll_module);
    size_t offset = 0;
    while (offset < actual_size_to_read) {
      uint8 unused = *(touch + offset);
      offset += step_size;
    }
    FreeLibrary(dll_module);
  }

  return true;
}

}  // namespace file_util
