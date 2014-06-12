/*-
 * Copyright (c) 2003-2010 Tim Kientzle
 * Copyright (c) 2012 Michihiro NAKAJIMA
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "archive_platform.h"
__FBSDID("$FreeBSD$");

#if !defined(_WIN32) || defined(__CYGWIN__)

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_ACL_H
#include <sys/acl.h>
#endif
#ifdef HAVE_SYS_EXTATTR_H
#include <sys/extattr.h>
#endif
#if defined(HAVE_SYS_XATTR_H)
#include <sys/xattr.h>
#elif defined(HAVE_ATTR_XATTR_H)
#include <attr/xattr.h>
#endif
#ifdef HAVE_SYS_EA_H
#include <sys/ea.h>
#endif
#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_SYS_UTIME_H
#include <sys/utime.h>
#endif
#ifdef HAVE_COPYFILE_H
#include <copyfile.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_GRP_H
#include <grp.h>
#endif
#ifdef HAVE_LANGINFO_H
#include <langinfo.h>
#endif
#ifdef HAVE_LINUX_FS_H
#include <linux/fs.h>	/* for Linux file flags */
#endif
/*
 * Some Linux distributions have both linux/ext2_fs.h and ext2fs/ext2_fs.h.
 * As the include guards don't agree, the order of include is important.
 */
#ifdef HAVE_LINUX_EXT2_FS_H
#include <linux/ext2_fs.h>	/* for Linux file flags */
#endif
#if defined(HAVE_EXT2FS_EXT2_FS_H) && !defined(__CYGWIN__)
#include <ext2fs/ext2_fs.h>	/* Linux file flags, broken on Cygwin */
#endif
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif
#ifdef HAVE_PWD_H
#include <pwd.h>
#endif
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_UTIME_H
#include <utime.h>
#endif
#ifdef F_GETTIMES /* Tru64 specific */
#include <sys/fcntl1.h>
#endif

#if __APPLE__
#include <TargetConditionals.h>
#if TARGET_OS_MAC && !TARGET_OS_EMBEDDED && HAVE_QUARANTINE_H
#include <quarantine.h>
#define HAVE_QUARANTINE 1
#endif
#endif

#ifdef HAVE_ZLIB_H
#include <zlib.h>
#endif

/* TODO: Support Mac OS 'quarantine' feature.  This is really just a
 * standard tag to mark files that have been downloaded as "tainted".
 * On Mac OS, we should mark the extracted files as tainted if the
 * archive being read was tainted.  Windows has a similar feature; we
 * should investigate ways to support this generically. */

#include "archive.h"
#include "archive_acl_private.h"
#include "archive_string.h"
#include "archive_endian.h"
#include "archive_entry.h"
#include "archive_private.h"
#include "archive_write_disk_private.h"

#ifndef O_BINARY
#define O_BINARY 0
#endif
#ifndef O_CLOEXEC
#define O_CLOEXEC	0
#endif

struct fixup_entry {
	struct fixup_entry	*next;
	struct archive_acl	 acl;
	mode_t			 mode;
	int64_t			 atime;
	int64_t                  birthtime;
	int64_t			 mtime;
	int64_t			 ctime;
	unsigned long		 atime_nanos;
	unsigned long            birthtime_nanos;
	unsigned long		 mtime_nanos;
	unsigned long		 ctime_nanos;
	unsigned long		 fflags_set;
	size_t			 mac_metadata_size;
	void			*mac_metadata;
	int			 fixup; /* bitmask of what needs fixing */
	char			*name;
};

/*
 * We use a bitmask to track which operations remain to be done for
 * this file.  In particular, this helps us avoid unnecessary
 * operations when it's possible to take care of one step as a
 * side-effect of another.  For example, mkdir() can specify the mode
 * for the newly-created object but symlink() cannot.  This means we
 * can skip chmod() if mkdir() succeeded, but we must explicitly
 * chmod() if we're trying to create a directory that already exists
 * (mkdir() failed) or if we're restoring a symlink.  Similarly, we
 * need to verify UID/GID before trying to restore SUID/SGID bits;
 * that verification can occur explicitly through a stat() call or
 * implicitly because of a successful chown() call.
 */
#define	TODO_MODE_FORCE		0x40000000
#define	TODO_MODE_BASE		0x20000000
#define	TODO_SUID		0x10000000
#define	TODO_SUID_CHECK		0x08000000
#define	TODO_SGID		0x04000000
#define	TODO_SGID_CHECK		0x02000000
#define	TODO_APPLEDOUBLE	0x01000000
#define	TODO_MODE		(TODO_MODE_BASE|TODO_SUID|TODO_SGID)
#define	TODO_TIMES		ARCHIVE_EXTRACT_TIME
#define	TODO_OWNER		ARCHIVE_EXTRACT_OWNER
#define	TODO_FFLAGS		ARCHIVE_EXTRACT_FFLAGS
#define	TODO_ACLS		ARCHIVE_EXTRACT_ACL
#define	TODO_XATTR		ARCHIVE_EXTRACT_XATTR
#define	TODO_MAC_METADATA	ARCHIVE_EXTRACT_MAC_METADATA
#define	TODO_HFS_COMPRESSION	ARCHIVE_EXTRACT_HFS_COMPRESSION_FORCED

struct archive_write_disk {
	struct archive	archive;

	mode_t			 user_umask;
	struct fixup_entry	*fixup_list;
	struct fixup_entry	*current_fixup;
	int64_t			 user_uid;
	int			 skip_file_set;
	int64_t			 skip_file_dev;
	int64_t			 skip_file_ino;
	time_t			 start_time;

	int64_t (*lookup_gid)(void *private, const char *gname, int64_t gid);
	void  (*cleanup_gid)(void *private);
	void			*lookup_gid_data;
	int64_t (*lookup_uid)(void *private, const char *uname, int64_t uid);
	void  (*cleanup_uid)(void *private);
	void			*lookup_uid_data;

	/*
	 * Full path of last file to satisfy symlink checks.
	 */
	struct archive_string	path_safe;

	/*
	 * Cached stat data from disk for the current entry.
	 * If this is valid, pst points to st.  Otherwise,
	 * pst is null.
	 */
	struct stat		 st;
	struct stat		*pst;

	/* Information about the object being restored right now. */
	struct archive_entry	*entry; /* Entry being extracted. */
	char			*name; /* Name of entry, possibly edited. */
	struct archive_string	 _name_data; /* backing store for 'name' */
	/* Tasks remaining for this object. */
	int			 todo;
	/* Tasks deferred until end-of-archive. */
	int			 deferred;
	/* Options requested by the client. */
	int			 flags;
	/* Handle for the file we're restoring. */
	int			 fd;
	/* Current offset for writing data to the file. */
	int64_t			 offset;
	/* Last offset actually written to disk. */
	int64_t			 fd_offset;
	/* Total bytes actually written to files. */
	int64_t			 total_bytes_written;
	/* Maximum size of file, -1 if unknown. */
	int64_t			 filesize;
	/* Dir we were in before this restore; only for deep paths. */
	int			 restore_pwd;
	/* Mode we should use for this entry; affected by _PERM and umask. */
	mode_t			 mode;
	/* UID/GID to use in restoring this entry. */
	int64_t			 uid;
	int64_t			 gid;
	/*
	 * HFS+ Compression.
	 */
	/* Xattr "com.apple.decmpfs". */
	uint32_t		 decmpfs_attr_size;
	unsigned char		*decmpfs_header_p;
	/* ResourceFork set options used for fsetxattr. */
	int			 rsrc_xattr_options;
	/* Xattr "com.apple.ResourceFork". */
	unsigned char		*resource_fork;
	size_t			 resource_fork_allocated_size;
	unsigned int		 decmpfs_block_count;
	uint32_t		*decmpfs_block_info;
	/* Buffer for compressed data. */
	unsigned char		*compressed_buffer;
	size_t			 compressed_buffer_size;
	size_t			 compressed_buffer_remaining;
	/* The offset of the ResourceFork where compressed data will
	 * be placed. */
	uint32_t		 compressed_rsrc_position;
	uint32_t		 compressed_rsrc_position_v;
	/* Buffer for uncompressed data. */
	char			*uncompressed_buffer;
	size_t			 block_remaining_bytes;
	size_t			 file_remaining_bytes;
#ifdef HAVE_ZLIB_H
	z_stream		 stream;
	int			 stream_valid;
	int			 decmpfs_compression_level;
#endif
};

/*
 * Default mode for dirs created automatically (will be modified by umask).
 * Note that POSIX specifies 0777 for implicitly-created dirs, "modified
 * by the process' file creation mask."
 */
#define	DEFAULT_DIR_MODE 0777
/*
 * Dir modes are restored in two steps:  During the extraction, the permissions
 * in the archive are modified to match the following limits.  During
 * the post-extract fixup pass, the permissions from the archive are
 * applied.
 */
#define	MINIMUM_DIR_MODE 0700
#define	MAXIMUM_DIR_MODE 0775

/*
 * Maxinum uncompressed size of a decmpfs block.
 */
#define MAX_DECMPFS_BLOCK_SIZE	(64 * 1024)
/*
 * HFS+ compression type.
 */
#define CMP_XATTR		3/* Compressed data in xattr. */
#define CMP_RESOURCE_FORK	4/* Compressed data in resource fork. */
/*
 * HFS+ compression resource fork.
 */
#define RSRC_H_SIZE	260	/* Base size of Resource fork header. */
#define RSRC_F_SIZE	50	/* Size of Resource fork footer. */
/* Size to write compressed data to resource fork. */
#define COMPRESSED_W_SIZE	(64 * 1024)
/* decmpfs difinitions. */
#define MAX_DECMPFS_XATTR_SIZE		3802
#ifndef DECMPFS_XATTR_NAME
#define DECMPFS_XATTR_NAME		"com.apple.decmpfs"
#endif
#define DECMPFS_MAGIC			0x636d7066
#define DECMPFS_COMPRESSION_MAGIC	0
#define DECMPFS_COMPRESSION_TYPE	4
#define DECMPFS_UNCOMPRESSED_SIZE	8
#define DECMPFS_HEADER_SIZE		16

#define HFS_BLOCKS(s)	((s) >> 12)

static int	check_symlinks(struct archive_write_disk *);
static int	create_filesystem_object(struct archive_write_disk *);
static struct fixup_entry *current_fixup(struct archive_write_disk *, const char *pathname);
#if defined(HAVE_FCHDIR) && defined(PATH_MAX)
static void	edit_deep_directories(struct archive_write_disk *ad);
#endif
static int	cleanup_pathname(struct archive_write_disk *);
static int	create_dir(struct archive_write_disk *, char *);
static int	create_parent_dir(struct archive_write_disk *, char *);
static ssize_t	hfs_write_data_block(struct archive_write_disk *,
		    const char *, size_t);
static int	fixup_appledouble(struct archive_write_disk *, const char *);
static int	older(struct stat *, struct archive_entry *);
static int	restore_entry(struct archive_write_disk *);
static int	set_mac_metadata(struct archive_write_disk *, const char *,
				 const void *, size_t);
static int	set_xattrs(struct archive_write_disk *);
static int	set_fflags(struct archive_write_disk *);
static int	set_fflags_platform(struct archive_write_disk *, int fd,
		    const char *name, mode_t mode,
		    unsigned long fflags_set, unsigned long fflags_clear);
static int	set_ownership(struct archive_write_disk *);
static int	set_mode(struct archive_write_disk *, int mode);
static int	set_time(int, int, const char *, time_t, long, time_t, long);
static int	set_times(struct archive_write_disk *, int, int, const char *,
		    time_t, long, time_t, long, time_t, long, time_t, long);
static int	set_times_from_entry(struct archive_write_disk *);
static struct fixup_entry *sort_dir_list(struct fixup_entry *p);
static ssize_t	write_data_block(struct archive_write_disk *,
		    const char *, size_t);

static struct archive_vtable *archive_write_disk_vtable(void);

static int	_archive_write_disk_close(struct archive *);
static int	_archive_write_disk_free(struct archive *);
static int	_archive_write_disk_header(struct archive *, struct archive_entry *);
static int64_t	_archive_write_disk_filter_bytes(struct archive *, int);
static int	_archive_write_disk_finish_entry(struct archive *);
static ssize_t	_archive_write_disk_data(struct archive *, const void *, size_t);
static ssize_t	_archive_write_disk_data_block(struct archive *, const void *, size_t, int64_t);

static int
lazy_stat(struct archive_write_disk *a)
{
	if (a->pst != NULL) {
		/* Already have stat() data available. */
		return (ARCHIVE_OK);
	}
#ifdef HAVE_FSTAT
	if (a->fd >= 0 && fstat(a->fd, &a->st) == 0) {
		a->pst = &a->st;
		return (ARCHIVE_OK);
	}
#endif
	/*
	 * XXX At this point, symlinks should not be hit, otherwise
	 * XXX a race occurred.  Do we want to check explicitly for that?
	 */
	if (lstat(a->name, &a->st) == 0) {
		a->pst = &a->st;
		return (ARCHIVE_OK);
	}
	archive_set_error(&a->archive, errno, "Couldn't stat file");
	return (ARCHIVE_WARN);
}

static struct archive_vtable *
archive_write_disk_vtable(void)
{
	static struct archive_vtable av;
	static int inited = 0;

	if (!inited) {
		av.archive_close = _archive_write_disk_close;
		av.archive_filter_bytes = _archive_write_disk_filter_bytes;
		av.archive_free = _archive_write_disk_free;
		av.archive_write_header = _archive_write_disk_header;
		av.archive_write_finish_entry
		    = _archive_write_disk_finish_entry;
		av.archive_write_data = _archive_write_disk_data;
		av.archive_write_data_block = _archive_write_disk_data_block;
		inited = 1;
	}
	return (&av);
}

static int64_t
_archive_write_disk_filter_bytes(struct archive *_a, int n)
{
	struct archive_write_disk *a = (struct archive_write_disk *)_a;
	(void)n; /* UNUSED */
	if (n == -1 || n == 0)
		return (a->total_bytes_written);
	return (-1);
}


int
archive_write_disk_set_options(struct archive *_a, int flags)
{
	struct archive_write_disk *a = (struct archive_write_disk *)_a;

	a->flags = flags;
	return (ARCHIVE_OK);
}


/*
 * Extract this entry to disk.
 *
 * TODO: Validate hardlinks.  According to the standards, we're
 * supposed to check each extracted hardlink and squawk if it refers
 * to a file that we didn't restore.  I'm not entirely convinced this
 * is a good idea, but more importantly: Is there any way to validate
 * hardlinks without keeping a complete list of filenames from the
 * entire archive?? Ugh.
 *
 */
static int
_archive_write_disk_header(struct archive *_a, struct archive_entry *entry)
{
	struct archive_write_disk *a = (struct archive_write_disk *)_a;
	struct fixup_entry *fe;
	int ret, r;

	archive_check_magic(&a->archive, ARCHIVE_WRITE_DISK_MAGIC,
	    ARCHIVE_STATE_HEADER | ARCHIVE_STATE_DATA,
	    "archive_write_disk_header");
	archive_clear_error(&a->archive);
	if (a->archive.state & ARCHIVE_STATE_DATA) {
		r = _archive_write_disk_finish_entry(&a->archive);
		if (r == ARCHIVE_FATAL)
			return (r);
	}

	/* Set up for this particular entry. */
	a->pst = NULL;
	a->current_fixup = NULL;
	a->deferred = 0;
	if (a->entry) {
		archive_entry_free(a->entry);
		a->entry = NULL;
	}
	a->entry = archive_entry_clone(entry);
	a->fd = -1;
	a->fd_offset = 0;
	a->offset = 0;
	a->restore_pwd = -1;
	a->uid = a->user_uid;
	a->mode = archive_entry_mode(a->entry);
	if (archive_entry_size_is_set(a->entry))
		a->filesize = archive_entry_size(a->entry);
	else
		a->filesize = -1;
	archive_strcpy(&(a->_name_data), archive_entry_pathname(a->entry));
	a->name = a->_name_data.s;
	archive_clear_error(&a->archive);

	/*
	 * Clean up the requested path.  This is necessary for correct
	 * dir restores; the dir restore logic otherwise gets messed
	 * up by nonsense like "dir/.".
	 */
	ret = cleanup_pathname(a);
	if (ret != ARCHIVE_OK)
		return (ret);

	/*
	 * Query the umask so we get predictable mode settings.
	 * This gets done on every call to _write_header in case the
	 * user edits their umask during the extraction for some
	 * reason.
	 */
	umask(a->user_umask = umask(0));

	/* Figure out what we need to do for this entry. */
	a->todo = TODO_MODE_BASE;
	if (a->flags & ARCHIVE_EXTRACT_PERM) {
		a->todo |= TODO_MODE_FORCE; /* Be pushy about permissions. */
		/*
		 * SGID requires an extra "check" step because we
		 * cannot easily predict the GID that the system will
		 * assign.  (Different systems assign GIDs to files
		 * based on a variety of criteria, including process
		 * credentials and the gid of the enclosing
		 * directory.)  We can only restore the SGID bit if
		 * the file has the right GID, and we only know the
		 * GID if we either set it (see set_ownership) or if
		 * we've actually called stat() on the file after it
		 * was restored.  Since there are several places at
		 * which we might verify the GID, we need a TODO bit
		 * to keep track.
		 */
		if (a->mode & S_ISGID)
			a->todo |= TODO_SGID | TODO_SGID_CHECK;
		/*
		 * Verifying the SUID is simpler, but can still be
		 * done in multiple ways, hence the separate "check" bit.
		 */
		if (a->mode & S_ISUID)
			a->todo |= TODO_SUID | TODO_SUID_CHECK;
	} else {
		/*
		 * User didn't request full permissions, so don't
		 * restore SUID, SGID bits and obey umask.
		 */
		a->mode &= ~S_ISUID;
		a->mode &= ~S_ISGID;
		a->mode &= ~S_ISVTX;
		a->mode &= ~a->user_umask;
	}
	if (a->flags & ARCHIVE_EXTRACT_OWNER)
		a->todo |= TODO_OWNER;
	if (a->flags & ARCHIVE_EXTRACT_TIME)
		a->todo |= TODO_TIMES;
	if (a->flags & ARCHIVE_EXTRACT_ACL) {
		if (archive_entry_filetype(a->entry) == AE_IFDIR)
			a->deferred |= TODO_ACLS;
		else
			a->todo |= TODO_ACLS;
	}
	if (a->flags & ARCHIVE_EXTRACT_MAC_METADATA) {
		if (archive_entry_filetype(a->entry) == AE_IFDIR)
			a->deferred |= TODO_MAC_METADATA;
		else
			a->todo |= TODO_MAC_METADATA;
	}
#if defined(__APPLE__) && defined(UF_COMPRESSED) && defined(HAVE_ZLIB_H)
	if ((a->flags & ARCHIVE_EXTRACT_NO_HFS_COMPRESSION) == 0) {
		unsigned long set, clear;
		archive_entry_fflags(a->entry, &set, &clear);
		if ((set & ~clear) & UF_COMPRESSED) {
			a->todo |= TODO_HFS_COMPRESSION;
			a->decmpfs_block_count = (unsigned)-1;
		}
	}
	if ((a->flags & ARCHIVE_EXTRACT_HFS_COMPRESSION_FORCED) != 0 &&
	    (a->mode & AE_IFMT) == AE_IFREG && a->filesize > 0) {
		a->todo |= TODO_HFS_COMPRESSION;
		a->decmpfs_block_count = (unsigned)-1;
	}
	{
		const char *p;

		/* Check if the current file name is a type of the
		 * resource fork file. */
		p = strrchr(a->name, '/');
		if (p == NULL)
			p = a->name;
		else
			p++;
		if (p[0] == '.' && p[1] == '_') {
			/* Do not compress "._XXX" files. */
			a->todo &= ~TODO_HFS_COMPRESSION;
			if (a->filesize > 0)
				a->todo |= TODO_APPLEDOUBLE;
		}
	}
#endif

	if (a->flags & ARCHIVE_EXTRACT_XATTR)
		a->todo |= TODO_XATTR;
	if (a->flags & ARCHIVE_EXTRACT_FFLAGS)
		a->todo |= TODO_FFLAGS;
	if (a->flags & ARCHIVE_EXTRACT_SECURE_SYMLINKS) {
		ret = check_symlinks(a);
		if (ret != ARCHIVE_OK)
			return (ret);
	}
#if defined(HAVE_FCHDIR) && defined(PATH_MAX)
	/* If path exceeds PATH_MAX, shorten the path. */
	edit_deep_directories(a);
#endif

	ret = restore_entry(a);

#if defined(__APPLE__) && defined(UF_COMPRESSED) && defined(HAVE_ZLIB_H)
	/*
	 * Check if the filesystem the file is restoring on supports
	 * HFS+ Compression. If not, cancel HFS+ Compression.
	 */
	if (a->todo | TODO_HFS_COMPRESSION) {
		/*
		 * NOTE: UF_COMPRESSED is ignored even if the filesystem
		 * supports HFS+ Compression because the file should
		 * have at least an extended attriute "com.apple.decmpfs"
		 * before the flag is set to indicate that the file have
		 * been compressed. If hte filesystem does not support
		 * HFS+ Compression the system call will fail.
		 */
		if (a->fd < 0 || fchflags(a->fd, UF_COMPRESSED) != 0)
			a->todo &= ~TODO_HFS_COMPRESSION;
	}
#endif

	/*
	 * TODO: There are rumours that some extended attributes must
	 * be restored before file data is written.  If this is true,
	 * then we either need to write all extended attributes both
	 * before and after restoring the data, or find some rule for
	 * determining which must go first and which last.  Due to the
	 * many ways people are using xattrs, this may prove to be an
	 * intractable problem.
	 */

#ifdef HAVE_FCHDIR
	/* If we changed directory above, restore it here. */
	if (a->restore_pwd >= 0) {
		r = fchdir(a->restore_pwd);
		if (r != 0) {
			archive_set_error(&a->archive, errno, "chdir() failure");
			ret = ARCHIVE_FATAL;
		}
		close(a->restore_pwd);
		a->restore_pwd = -1;
	}
#endif

	/*
	 * Fixup uses the unedited pathname from archive_entry_pathname(),
	 * because it is relative to the base dir and the edited path
	 * might be relative to some intermediate dir as a result of the
	 * deep restore logic.
	 */
	if (a->deferred & TODO_MODE) {
		fe = current_fixup(a, archive_entry_pathname(entry));
		if (fe == NULL)
			return (ARCHIVE_FATAL);
		fe->fixup |= TODO_MODE_BASE;
		fe->mode = a->mode;
	}

	if ((a->deferred & TODO_TIMES)
		&& (archive_entry_mtime_is_set(entry)
		    || archive_entry_atime_is_set(entry))) {
		fe = current_fixup(a, archive_entry_pathname(entry));
		if (fe == NULL)
			return (ARCHIVE_FATAL);
		fe->mode = a->mode;
		fe->fixup |= TODO_TIMES;
		if (archive_entry_atime_is_set(entry)) {
			fe->atime = archive_entry_atime(entry);
			fe->atime_nanos = archive_entry_atime_nsec(entry);
		} else {
			/* If atime is unset, use start time. */
			fe->atime = a->start_time;
			fe->atime_nanos = 0;
		}
		if (archive_entry_mtime_is_set(entry)) {
			fe->mtime = archive_entry_mtime(entry);
			fe->mtime_nanos = archive_entry_mtime_nsec(entry);
		} else {
			/* If mtime is unset, use start time. */
			fe->mtime = a->start_time;
			fe->mtime_nanos = 0;
		}
		if (archive_entry_birthtime_is_set(entry)) {
			fe->birthtime = archive_entry_birthtime(entry);
			fe->birthtime_nanos = archive_entry_birthtime_nsec(entry);
		} else {
			/* If birthtime is unset, use mtime. */
			fe->birthtime = fe->mtime;
			fe->birthtime_nanos = fe->mtime_nanos;
		}
	}

	if (a->deferred & TODO_ACLS) {
		fe = current_fixup(a, archive_entry_pathname(entry));
		if (fe == NULL)
			return (ARCHIVE_FATAL);
		fe->fixup |= TODO_ACLS;
		archive_acl_copy(&fe->acl, archive_entry_acl(entry));
	}

	if (a->deferred & TODO_MAC_METADATA) {
		const void *metadata;
		size_t metadata_size;
		metadata = archive_entry_mac_metadata(a->entry, &metadata_size);
		if (metadata != NULL && metadata_size > 0) {
			fe = current_fixup(a, archive_entry_pathname(entry));
			if (fe == NULL)
				return (ARCHIVE_FATAL);
			fe->mac_metadata = malloc(metadata_size);
			if (fe->mac_metadata != NULL) {
				memcpy(fe->mac_metadata, metadata, metadata_size);
				fe->mac_metadata_size = metadata_size;
				fe->fixup |= TODO_MAC_METADATA;
			}
		}
	}

	if (a->deferred & TODO_FFLAGS) {
		fe = current_fixup(a, archive_entry_pathname(entry));
		if (fe == NULL)
			return (ARCHIVE_FATAL);
		fe->fixup |= TODO_FFLAGS;
		/* TODO: Complete this.. defer fflags from below. */
	}

	/* We've created the object and are ready to pour data into it. */
	if (ret >= ARCHIVE_WARN)
		a->archive.state = ARCHIVE_STATE_DATA;
	/*
	 * If it's not open, tell our client not to try writing.
	 * In particular, dirs, links, etc, don't get written to.
	 */
	if (a->fd < 0) {
		archive_entry_set_size(entry, 0);
		a->filesize = 0;
	}

	return (ret);
}

int
archive_write_disk_set_skip_file(struct archive *_a, int64_t d, int64_t i)
{
	struct archive_write_disk *a = (struct archive_write_disk *)_a;
	archive_check_magic(&a->archive, ARCHIVE_WRITE_DISK_MAGIC,
	    ARCHIVE_STATE_ANY, "archive_write_disk_set_skip_file");
	a->skip_file_set = 1;
	a->skip_file_dev = d;
	a->skip_file_ino = i;
	return (ARCHIVE_OK);
}

static ssize_t
write_data_block(struct archive_write_disk *a, const char *buff, size_t size)
{
	uint64_t start_size = size;
	ssize_t bytes_written = 0;
	ssize_t block_size = 0, bytes_to_write;

	if (size == 0)
		return (ARCHIVE_OK);

	if (a->filesize == 0 || a->fd < 0) {
		archive_set_error(&a->archive, 0,
		    "Attempt to write to an empty file");
		return (ARCHIVE_WARN);
	}

	if (a->flags & ARCHIVE_EXTRACT_SPARSE) {
#if HAVE_STRUCT_STAT_ST_BLKSIZE
		int r;
		if ((r = lazy_stat(a)) != ARCHIVE_OK)
			return (r);
		block_size = a->pst->st_blksize;
#else
		/* XXX TODO XXX Is there a more appropriate choice here ? */
		/* This needn't match the filesystem allocation size. */
		block_size = 16*1024;
#endif
	}

	/* If this write would run beyond the file size, truncate it. */
	if (a->filesize >= 0 && (int64_t)(a->offset + size) > a->filesize)
		start_size = size = (size_t)(a->filesize - a->offset);

	/* Write the data. */
	while (size > 0) {
		if (block_size == 0) {
			bytes_to_write = size;
		} else {
			/* We're sparsifying the file. */
			const char *p, *end;
			int64_t block_end;

			/* Skip leading zero bytes. */
			for (p = buff, end = buff + size; p < end; ++p) {
				if (*p != '\0')
					break;
			}
			a->offset += p - buff;
			size -= p - buff;
			buff = p;
			if (size == 0)
				break;

			/* Calculate next block boundary after offset. */
			block_end
			    = (a->offset / block_size + 1) * block_size;

			/* If the adjusted write would cross block boundary,
			 * truncate it to the block boundary. */
			bytes_to_write = size;
			if (a->offset + bytes_to_write > block_end)
				bytes_to_write = block_end - a->offset;
		}
		/* Seek if necessary to the specified offset. */
		if (a->offset != a->fd_offset) {
			if (lseek(a->fd, a->offset, SEEK_SET) < 0) {
				archive_set_error(&a->archive, errno,
				    "Seek failed");
				return (ARCHIVE_FATAL);
			}
			a->fd_offset = a->offset;
		}
		bytes_written = write(a->fd, buff, bytes_to_write);
		if (bytes_written < 0) {
			archive_set_error(&a->archive, errno, "Write failed");
			return (ARCHIVE_WARN);
		}
		buff += bytes_written;
		size -= bytes_written;
		a->total_bytes_written += bytes_written;
		a->offset += bytes_written;
		a->fd_offset = a->offset;
	}
	return (start_size - size);
}

#if defined(__APPLE__) && defined(UF_COMPRESSED) && defined(HAVE_SYS_XATTR_H)\
	&& defined(HAVE_ZLIB_H)

/*
 * Set UF_COMPRESSED file flag.
 * This have to be called after hfs_write_decmpfs() because if the
 * file does not have "com.apple.decmpfs" xattr the flag is ignored.
 */
static int
hfs_set_compressed_fflag(struct archive_write_disk *a)
{
	int r;

	if ((r = lazy_stat(a)) != ARCHIVE_OK)
		return (r);

	a->st.st_flags |= UF_COMPRESSED;
	if (fchflags(a->fd, a->st.st_flags) != 0) {
		archive_set_error(&a->archive, errno,
		    "Failed to set UF_COMPRESSED file flag");
		return (ARCHIVE_WARN);
	}
	return (ARCHIVE_OK);
}

/*
 * HFS+ Compression decmpfs
 *
 *     +------------------------------+ +0
 *     |      Magic(LE 4 bytes)       |
 *     +------------------------------+
 *     |      Type(LE 4 bytes)        |
 *     +------------------------------+
 *     | Uncompressed size(LE 8 bytes)|
 *     +------------------------------+ +16
 *     |                              |
 *     |       Compressed data        |
 *     |  (Placed only if Type == 3)  |
 *     |                              |
 *     +------------------------------+  +3802 = MAX_DECMPFS_XATTR_SIZE
 *
 *  Type is 3: decmpfs has compressed data.
 *  Type is 4: Resource Fork has compressed data.
 */
/*
 * Write "com.apple.decmpfs"
 */
static int
hfs_write_decmpfs(struct archive_write_disk *a)
{
	int r;
	uint32_t compression_type;

	r = fsetxattr(a->fd, DECMPFS_XATTR_NAME, a->decmpfs_header_p,
	    a->decmpfs_attr_size, 0, 0);
	if (r < 0) {
		archive_set_error(&a->archive, errno,
		    "Cannot restore xattr:%s", DECMPFS_XATTR_NAME);
		compression_type = archive_le32dec(
		    &a->decmpfs_header_p[DECMPFS_COMPRESSION_TYPE]);
		if (compression_type == CMP_RESOURCE_FORK)
			fremovexattr(a->fd, XATTR_RESOURCEFORK_NAME,
			    XATTR_SHOWCOMPRESSION);
		return (ARCHIVE_WARN);
	}
	return (ARCHIVE_OK);
}

/*
 * HFS+ Compression Resource Fork
 *
 *     +-----------------------------+
 *     |     Header(260 bytes)       |
 *     +-----------------------------+
 *     |   Block count(LE 4 bytes)   |
 *     +-----------------------------+  --+
 * +-- |     Offset (LE 4 bytes)     |    |
 * |   | [distance from Block count] |    | Block 0
 * |   +-----------------------------+    |
 * |   | Compressed size(LE 4 bytes) |    |
 * |   +-----------------------------+  --+
 * |   |                             |
 * |   |      ..................     |
 * |   |                             |
 * |   +-----------------------------+  --+
 * |   |     Offset (LE 4 bytes)     |    |
 * |   +-----------------------------+    | Block (Block count -1)
 * |   | Compressed size(LE 4 bytes) |    |
 * +-> +-----------------------------+  --+
 *     |   Compressed data(n bytes)  |  Block 0
 *     +-----------------------------+
 *     |                             |
 *     |      ..................     |
 *     |                             |
 *     +-----------------------------+
 *     |   Compressed data(n bytes)  |  Block (Block count -1)
 *     +-----------------------------+
 *     |      Footer(50 bytes)       |
 *     +-----------------------------+
 *
 */
/*
 * Write the header of "com.apple.ResourceFork"
 */
static int
hfs_write_resource_fork(struct archive_write_disk *a, unsigned char *buff,
    size_t bytes, uint32_t position)
{
	int ret;

	ret = fsetxattr(a->fd, XATTR_RESOURCEFORK_NAME, buff, bytes,
	    position, a->rsrc_xattr_options);
	if (ret < 0) {
		archive_set_error(&a->archive, errno,
		    "Cannot restore xattr: %s at %u pos %u bytes",
		    XATTR_RESOURCEFORK_NAME,
		    (unsigned)position,
		    (unsigned)bytes);
		return (ARCHIVE_WARN);
	}
	a->rsrc_xattr_options &= ~XATTR_CREATE;
	return (ARCHIVE_OK);
}

static int
hfs_write_compressed_data(struct archive_write_disk *a, size_t bytes_compressed)
{
	int ret;

	ret = hfs_write_resource_fork(a, a->compressed_buffer,
	    bytes_compressed, a->compressed_rsrc_position);
	if (ret == ARCHIVE_OK)
		a->compressed_rsrc_position += bytes_compressed;
	return (ret);
}

static int
hfs_write_resource_fork_header(struct archive_write_disk *a)
{
	unsigned char *buff;
	uint32_t rsrc_bytes;
	uint32_t rsrc_header_bytes;

	/*
	 * Write resource fork header + block info.
	 */
	buff = a->resource_fork;
	rsrc_bytes = a->compressed_rsrc_position - RSRC_F_SIZE;
	rsrc_header_bytes =
		RSRC_H_SIZE +		/* Header base size. */
		4 +			/* Block count. */
		(a->decmpfs_block_count * 8);/* Block info */
	archive_be32enc(buff, 0x100);
	archive_be32enc(buff + 4, rsrc_bytes);
	archive_be32enc(buff + 8, rsrc_bytes - 256);
	archive_be32enc(buff + 12, 0x32);
	memset(buff + 16, 0, 240);
	archive_be32enc(buff + 256, rsrc_bytes - 260);
	return hfs_write_resource_fork(a, buff, rsrc_header_bytes, 0);
}

static size_t
hfs_set_resource_fork_footer(unsigned char *buff, size_t buff_size)
{
	static const char rsrc_footer[RSRC_F_SIZE] = {
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x1c, 0x00, 0x32, 0x00, 0x00, 'c',  'm',
		'p', 'f',   0x00, 0x00, 0x00, 0x0a, 0x00, 0x01,
		0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00
	};
	if (buff_size < sizeof(rsrc_footer))
		return (0);
	memcpy(buff, rsrc_footer, sizeof(rsrc_footer));
	return (sizeof(rsrc_footer));
}

static int
hfs_reset_compressor(struct archive_write_disk *a)
{
	int ret;

	if (a->stream_valid)
		ret = deflateReset(&a->stream);
	else
		ret = deflateInit(&a->stream, a->decmpfs_compression_level);

	if (ret != Z_OK) {
		archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
		    "Failed to initialize compressor");
		return (ARCHIVE_FATAL);
	} else
		a->stream_valid = 1;

	return (ARCHIVE_OK);
}

static int
hfs_decompress(struct archive_write_disk *a)
{
	uint32_t *block_info;
	unsigned int block_count;
	uint32_t data_pos, data_size;
	ssize_t r;
	ssize_t bytes_written, bytes_to_write;
	unsigned char *b;

	block_info = (uint32_t *)(a->resource_fork + RSRC_H_SIZE);
	block_count = archive_le32dec(block_info++);
	while (block_count--) {
		data_pos = RSRC_H_SIZE + archive_le32dec(block_info++);
		data_size = archive_le32dec(block_info++);
		r = fgetxattr(a->fd, XATTR_RESOURCEFORK_NAME,
		    a->compressed_buffer, data_size, data_pos, 0);
		if (r != data_size)  {
			archive_set_error(&a->archive,
			    (r < 0)?errno:ARCHIVE_ERRNO_MISC,
			    "Failed to read resource fork");
			return (ARCHIVE_WARN);
		}
		if (a->compressed_buffer[0] == 0xff) {
			bytes_to_write = data_size -1;
			b = a->compressed_buffer + 1;
		} else {
			uLong dest_len = MAX_DECMPFS_BLOCK_SIZE;
			int zr;

			zr = uncompress((Bytef *)a->uncompressed_buffer,
			    &dest_len, a->compressed_buffer, data_size);
			if (zr != Z_OK) {
				archive_set_error(&a->archive,
				    ARCHIVE_ERRNO_MISC,
				    "Failed to decompress resource fork");
				return (ARCHIVE_WARN);
			}
			bytes_to_write = dest_len;
			b = (unsigned char *)a->uncompressed_buffer;
		}
		do {
			bytes_written = write(a->fd, b, bytes_to_write);
			if (bytes_written < 0) {
				archive_set_error(&a->archive, errno,
				    "Write failed");
				return (ARCHIVE_WARN);
			}
			bytes_to_write -= bytes_written;
			b += bytes_written;
		} while (bytes_to_write > 0);
	}
	r = fremovexattr(a->fd, XATTR_RESOURCEFORK_NAME, 0);
	if (r == -1)  {
		archive_set_error(&a->archive, errno,
		    "Failed to remove resource fork");
		return (ARCHIVE_WARN);
	}
	return (ARCHIVE_OK);
}

static int
hfs_drive_compressor(struct archive_write_disk *a, const char *buff,
    size_t size)
{
	unsigned char *buffer_compressed;
	size_t bytes_compressed;
	size_t bytes_used;
	int ret;

	ret = hfs_reset_compressor(a);
	if (ret != ARCHIVE_OK)
		return (ret);

	if (a->compressed_buffer == NULL) {
		size_t block_size;

		block_size = COMPRESSED_W_SIZE + RSRC_F_SIZE +
		    + compressBound(MAX_DECMPFS_BLOCK_SIZE);
		a->compressed_buffer = malloc(block_size);
		if (a->compressed_buffer == NULL) {
			archive_set_error(&a->archive, ENOMEM,
			    "Can't allocate memory for Resource Fork");
			return (ARCHIVE_FATAL);
		}
		a->compressed_buffer_size = block_size;
		a->compressed_buffer_remaining = block_size;
	}

	buffer_compressed = a->compressed_buffer +
	    a->compressed_buffer_size - a->compressed_buffer_remaining;
	a->stream.next_in = (Bytef *)(uintptr_t)(const void *)buff;
	a->stream.avail_in = size;
	a->stream.next_out = buffer_compressed;
	a->stream.avail_out = a->compressed_buffer_remaining;
	do {
		ret = deflate(&a->stream, Z_FINISH);
		switch (ret) {
		case Z_OK:
		case Z_STREAM_END:
			break;
		default:
			archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
			    "Failed to compress data");
			return (ARCHIVE_FAILED);
		}
	} while (ret == Z_OK);
	bytes_compressed = a->compressed_buffer_remaining - a->stream.avail_out;

	/*
	 * If the compressed size is larger than the original size,
	 * throw away compressed data, use uncompressed data instead.
	 */
	if (bytes_compressed > size) {
		buffer_compressed[0] = 0xFF;/* uncompressed marker. */
		memcpy(buffer_compressed + 1, buff, size);
		bytes_compressed = size + 1;
	}
	a->compressed_buffer_remaining -= bytes_compressed;

	/*
	 * If the compressed size is smaller than MAX_DECMPFS_XATTR_SIZE
	 * and the block count in the file is only one, store compressed
	 * data to decmpfs xattr instead of the resource fork.
	 */
	if (a->decmpfs_block_count == 1 &&
	    (a->decmpfs_attr_size + bytes_compressed)
	      <= MAX_DECMPFS_XATTR_SIZE) {
		archive_le32enc(&a->decmpfs_header_p[DECMPFS_COMPRESSION_TYPE],
		    CMP_XATTR);
		memcpy(a->decmpfs_header_p + DECMPFS_HEADER_SIZE,
		    buffer_compressed, bytes_compressed);
		a->decmpfs_attr_size += bytes_compressed;
		a->compressed_buffer_remaining = a->compressed_buffer_size;
		/*
		 * Finish HFS+ Compression.
		 * - Write the decmpfs xattr.
		 * - Set the UF_COMPRESSED file flag.
		 */
		ret = hfs_write_decmpfs(a);
		if (ret == ARCHIVE_OK)
			ret = hfs_set_compressed_fflag(a);
		return (ret);
	}

	/* Update block info. */
	archive_le32enc(a->decmpfs_block_info++,
	    a->compressed_rsrc_position_v - RSRC_H_SIZE);
	archive_le32enc(a->decmpfs_block_info++, bytes_compressed);
	a->compressed_rsrc_position_v += bytes_compressed;

	/*
	 * Write the compressed data to the resource fork.
	 */
	bytes_used = a->compressed_buffer_size - a->compressed_buffer_remaining;
	while (bytes_used >= COMPRESSED_W_SIZE) {
		ret = hfs_write_compressed_data(a, COMPRESSED_W_SIZE);
		if (ret != ARCHIVE_OK)
			return (ret);
		bytes_used -= COMPRESSED_W_SIZE;
		if (bytes_used > COMPRESSED_W_SIZE)
			memmove(a->compressed_buffer,
			    a->compressed_buffer + COMPRESSED_W_SIZE,
			    bytes_used);
		else
			memcpy(a->compressed_buffer,
			    a->compressed_buffer + COMPRESSED_W_SIZE,
			    bytes_used);
	}
	a->compressed_buffer_remaining = a->compressed_buffer_size - bytes_used;

	/*
	 * If the current block is the last block, write the remaining
	 * compressed data and the resource fork footer.
	 */
	if (a->file_remaining_bytes == 0) {
		size_t rsrc_size;
		int64_t bk;

		/* Append the resource footer. */
		rsrc_size = hfs_set_resource_fork_footer(
		    a->compressed_buffer + bytes_used,
		    a->compressed_buffer_remaining);
		ret = hfs_write_compressed_data(a, bytes_used + rsrc_size);
		a->compressed_buffer_remaining = a->compressed_buffer_size;

		/* If the compressed size is not enouph smaller than
		 * the uncompressed size. cancel HFS+ compression.
		 * TODO: study a behavior of ditto utility and improve
		 * the condition to fall back into no HFS+ compression. */
		bk = HFS_BLOCKS(a->compressed_rsrc_position);
		bk += bk >> 7;
		if (bk > HFS_BLOCKS(a->filesize))
			return hfs_decompress(a);
		/*
		 * Write the resourcefork header.
		 */
		if (ret == ARCHIVE_OK)
			ret = hfs_write_resource_fork_header(a);
		/*
		 * Finish HFS+ Compression.
		 * - Write the decmpfs xattr.
		 * - Set the UF_COMPRESSED file flag.
		 */
		if (ret == ARCHIVE_OK)
			ret = hfs_write_decmpfs(a);
		if (ret == ARCHIVE_OK)
			ret = hfs_set_compressed_fflag(a);
	}
	return (ret);
}

static ssize_t
hfs_write_decmpfs_block(struct archive_write_disk *a, const char *buff,
    size_t size)
{
	const char *buffer_to_write;
	size_t bytes_to_write;
	int ret;

	if (a->decmpfs_block_count == (unsigned)-1) {
		void *new_block;
		size_t new_size;
		unsigned int block_count;

		if (a->decmpfs_header_p == NULL) {
			new_block = malloc(MAX_DECMPFS_XATTR_SIZE
			    + sizeof(uint32_t));
			if (new_block == NULL) {
				archive_set_error(&a->archive, ENOMEM,
				    "Can't allocate memory for decmpfs");
				return (ARCHIVE_FATAL);
			}
			a->decmpfs_header_p = new_block;
		}
		a->decmpfs_attr_size = DECMPFS_HEADER_SIZE;
		archive_le32enc(&a->decmpfs_header_p[DECMPFS_COMPRESSION_MAGIC],
		    DECMPFS_MAGIC);
		archive_le32enc(&a->decmpfs_header_p[DECMPFS_COMPRESSION_TYPE],
		    CMP_RESOURCE_FORK);
		archive_le64enc(&a->decmpfs_header_p[DECMPFS_UNCOMPRESSED_SIZE],
		    a->filesize);

		/* Calculate a block count of the file. */
		block_count =
		    (a->filesize + MAX_DECMPFS_BLOCK_SIZE -1) /
			MAX_DECMPFS_BLOCK_SIZE;
		/*
		 * Allocate buffer for resource fork.
		 * Set up related pointers;
		 */
		new_size =
		    RSRC_H_SIZE + /* header */
		    4 + /* Block count */
		    (block_count * sizeof(uint32_t) * 2) +
		    RSRC_F_SIZE; /* footer */
		if (new_size > a->resource_fork_allocated_size) {
			new_block = realloc(a->resource_fork, new_size);
			if (new_block == NULL) {
				archive_set_error(&a->archive, ENOMEM,
				    "Can't allocate memory for ResourceFork");
				return (ARCHIVE_FATAL);
			}
			a->resource_fork_allocated_size = new_size;
			a->resource_fork = new_block;
		}

		/* Allocate uncompressed buffer */
		if (a->uncompressed_buffer == NULL) {
			new_block = malloc(MAX_DECMPFS_BLOCK_SIZE);
			if (new_block == NULL) {
				archive_set_error(&a->archive, ENOMEM,
				    "Can't allocate memory for decmpfs");
				return (ARCHIVE_FATAL);
			}
			a->uncompressed_buffer = new_block;
		}
		a->block_remaining_bytes = MAX_DECMPFS_BLOCK_SIZE;
		a->file_remaining_bytes = a->filesize;
		a->compressed_buffer_remaining = a->compressed_buffer_size;

		/*
		 * Set up a resource fork.
		 */
		a->rsrc_xattr_options = XATTR_CREATE;
		/* Get the position where we are going to set a bunch
		 * of block info. */
		a->decmpfs_block_info =
		    (uint32_t *)(a->resource_fork + RSRC_H_SIZE);
		/* Set the block count to the resource fork. */
		archive_le32enc(a->decmpfs_block_info++, block_count);
		/* Get the position where we are goint to set compressed
		 * data. */
		a->compressed_rsrc_position =
		    RSRC_H_SIZE + 4 + (block_count * 8);
		a->compressed_rsrc_position_v = a->compressed_rsrc_position;
		a->decmpfs_block_count = block_count;
	}

	/* Ignore redundant bytes. */
	if (a->file_remaining_bytes == 0)
		return ((ssize_t)size);

	/* Do not overrun a block size. */
	if (size > a->block_remaining_bytes)
		bytes_to_write = a->block_remaining_bytes;
	else
		bytes_to_write = size;
	/* Do not overrun the file size. */
	if (bytes_to_write > a->file_remaining_bytes)
		bytes_to_write = a->file_remaining_bytes;

	/* For efficiency, if a copy length is full of the uncompressed
	 * buffer size, do not copy writing data to it. */
	if (bytes_to_write == MAX_DECMPFS_BLOCK_SIZE)
		buffer_to_write = buff;
	else {
		memcpy(a->uncompressed_buffer +
		    MAX_DECMPFS_BLOCK_SIZE - a->block_remaining_bytes,
		    buff, bytes_to_write);
		buffer_to_write = a->uncompressed_buffer;
	}
	a->block_remaining_bytes -= bytes_to_write;
	a->file_remaining_bytes -= bytes_to_write;

	if (a->block_remaining_bytes == 0 || a->file_remaining_bytes == 0) {
		ret = hfs_drive_compressor(a, buffer_to_write,
		    MAX_DECMPFS_BLOCK_SIZE - a->block_remaining_bytes);
		if (ret < 0)
			return (ret);
		a->block_remaining_bytes = MAX_DECMPFS_BLOCK_SIZE;
	}
	/* Ignore redundant bytes. */
	if (a->file_remaining_bytes == 0)
		return ((ssize_t)size);
	return (bytes_to_write);
}

static ssize_t
hfs_write_data_block(struct archive_write_disk *a, const char *buff,
    size_t size)
{
	uint64_t start_size = size;
	ssize_t bytes_written = 0;
	ssize_t bytes_to_write;

	if (size == 0)
		return (ARCHIVE_OK);

	if (a->filesize == 0 || a->fd < 0) {
		archive_set_error(&a->archive, 0,
		    "Attempt to write to an empty file");
		return (ARCHIVE_WARN);
	}

	/* If this write would run beyond the file size, truncate it. */
	if (a->filesize >= 0 && (int64_t)(a->offset + size) > a->filesize)
		start_size = size = (size_t)(a->filesize - a->offset);

	/* Write the data. */
	while (size > 0) {
		bytes_to_write = size;
		/* Seek if necessary to the specified offset. */
		if (a->offset < a->fd_offset) {
			/* Can't support backword move. */
			archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
			    "Seek failed");
			return (ARCHIVE_FATAL);
		} else if (a->offset > a->fd_offset) {
			int64_t skip = a->offset - a->fd_offset;
			char nullblock[1024];

			memset(nullblock, 0, sizeof(nullblock));
			while (skip > 0) {
				if (skip > (int64_t)sizeof(nullblock))
					bytes_written = hfs_write_decmpfs_block(
					    a, nullblock, sizeof(nullblock));
				else
					bytes_written = hfs_write_decmpfs_block(
					    a, nullblock, skip);
				if (bytes_written < 0) {
					archive_set_error(&a->archive, errno,
					    "Write failed");
					return (ARCHIVE_WARN);
				}
				skip -= bytes_written;
			}

			a->fd_offset = a->offset;
		}
		bytes_written =
		    hfs_write_decmpfs_block(a, buff, bytes_to_write);
		if (bytes_written < 0)
			return (bytes_written);
		buff += bytes_written;
		size -= bytes_written;
		a->total_bytes_written += bytes_written;
		a->offset += bytes_written;
		a->fd_offset = a->offset;
	}
	return (start_size - size);
}
#else
static ssize_t
hfs_write_data_block(struct archive_write_disk *a, const char *buff,
    size_t size)
{
	return (write_data_block(a, buff, size));
}
#endif

static ssize_t
_archive_write_disk_data_block(struct archive *_a,
    const void *buff, size_t size, int64_t offset)
{
	struct archive_write_disk *a = (struct archive_write_disk *)_a;
	ssize_t r;

	archive_check_magic(&a->archive, ARCHIVE_WRITE_DISK_MAGIC,
	    ARCHIVE_STATE_DATA, "archive_write_data_block");

	a->offset = offset;
	if (a->todo & TODO_HFS_COMPRESSION)
		r = hfs_write_data_block(a, buff, size);
	else
		r = write_data_block(a, buff, size);
	if (r < ARCHIVE_OK)
		return (r);
	if ((size_t)r < size) {
		archive_set_error(&a->archive, 0,
		    "Write request too large");
		return (ARCHIVE_WARN);
	}
	return (ARCHIVE_OK);
}

static ssize_t
_archive_write_disk_data(struct archive *_a, const void *buff, size_t size)
{
	struct archive_write_disk *a = (struct archive_write_disk *)_a;

	archive_check_magic(&a->archive, ARCHIVE_WRITE_DISK_MAGIC,
	    ARCHIVE_STATE_DATA, "archive_write_data");

	if (a->todo & TODO_HFS_COMPRESSION)
		return (hfs_write_data_block(a, buff, size));
	return (write_data_block(a, buff, size));
}

static int
_archive_write_disk_finish_entry(struct archive *_a)
{
	struct archive_write_disk *a = (struct archive_write_disk *)_a;
	int ret = ARCHIVE_OK;

	archive_check_magic(&a->archive, ARCHIVE_WRITE_DISK_MAGIC,
	    ARCHIVE_STATE_HEADER | ARCHIVE_STATE_DATA,
	    "archive_write_finish_entry");
	if (a->archive.state & ARCHIVE_STATE_HEADER)
		return (ARCHIVE_OK);
	archive_clear_error(&a->archive);

	/* Pad or truncate file to the right size. */
	if (a->fd < 0) {
		/* There's no file. */
	} else if (a->filesize < 0) {
		/* File size is unknown, so we can't set the size. */
	} else if (a->fd_offset == a->filesize) {
		/* Last write ended at exactly the filesize; we're done. */
		/* Hopefully, this is the common case. */
#if defined(__APPLE__) && defined(UF_COMPRESSED) && defined(HAVE_ZLIB_H)
	} else if (a->todo & TODO_HFS_COMPRESSION) {
		char null_d[1024];
		ssize_t r;

		if (a->file_remaining_bytes)
			memset(null_d, 0, sizeof(null_d));
		while (a->file_remaining_bytes) {
			if (a->file_remaining_bytes > sizeof(null_d))
				r = hfs_write_data_block(
				    a, null_d, sizeof(null_d));
			else
				r = hfs_write_data_block(
				    a, null_d, a->file_remaining_bytes);
			if (r < 0)
				return ((int)r);
		}
#endif
	} else {
#if HAVE_FTRUNCATE
		if (ftruncate(a->fd, a->filesize) == -1 &&
		    a->filesize == 0) {
			archive_set_error(&a->archive, errno,
			    "File size could not be restored");
			return (ARCHIVE_FAILED);
		}
#endif
		/*
		 * Not all platforms implement the XSI option to
		 * extend files via ftruncate.  Stat() the file again
		 * to see what happened.
		 */
		a->pst = NULL;
		if ((ret = lazy_stat(a)) != ARCHIVE_OK)
			return (ret);
		/* We can use lseek()/write() to extend the file if
		 * ftruncate didn't work or isn't available. */
		if (a->st.st_size < a->filesize) {
			const char nul = '\0';
			if (lseek(a->fd, a->filesize - 1, SEEK_SET) < 0) {
				archive_set_error(&a->archive, errno,
				    "Seek failed");
				return (ARCHIVE_FATAL);
			}
			if (write(a->fd, &nul, 1) < 0) {
				archive_set_error(&a->archive, errno,
				    "Write to restore size failed");
				return (ARCHIVE_FATAL);
			}
			a->pst = NULL;
		}
	}

	/* Restore metadata. */

	/*
	 * This is specific to Mac OS X.
	 * If the current file is an AppleDouble file, it should be
	 * linked with the data fork file and remove it.
	 */
	if (a->todo & TODO_APPLEDOUBLE) {
		int r2 = fixup_appledouble(a, a->name);
		if (r2 == ARCHIVE_EOF) {
			/* The current file has been successfully linked
			 * with the data fork file and removed. So there
			 * is nothing to do on the current file.  */
			goto finish_metadata;
		}
		if (r2 < ret) ret = r2;
	}

	/*
	 * Look up the "real" UID only if we're going to need it.
	 * TODO: the TODO_SGID condition can be dropped here, can't it?
	 */
	if (a->todo & (TODO_OWNER | TODO_SUID | TODO_SGID)) {
		a->uid = archive_write_disk_uid(&a->archive,
		    archive_entry_uname(a->entry),
		    archive_entry_uid(a->entry));
	}
	/* Look up the "real" GID only if we're going to need it. */
	/* TODO: the TODO_SUID condition can be dropped here, can't it? */
	if (a->todo & (TODO_OWNER | TODO_SGID | TODO_SUID)) {
		a->gid = archive_write_disk_gid(&a->archive,
		    archive_entry_gname(a->entry),
		    archive_entry_gid(a->entry));
	 }

	/*
	 * Restore ownership before set_mode tries to restore suid/sgid
	 * bits.  If we set the owner, we know what it is and can skip
	 * a stat() call to examine the ownership of the file on disk.
	 */
	if (a->todo & TODO_OWNER) {
		int r2 = set_ownership(a);
		if (r2 < ret) ret = r2;
	}

	/*
	 * set_mode must precede ACLs on systems such as Solaris and
	 * FreeBSD where setting the mode implicitly clears extended ACLs
	 */
	if (a->todo & TODO_MODE) {
		int r2 = set_mode(a, a->mode);
		if (r2 < ret) ret = r2;
	}

	/*
	 * Security-related extended attributes (such as
	 * security.capability on Linux) have to be restored last,
	 * since they're implicitly removed by other file changes.
	 */
	if (a->todo & TODO_XATTR) {
		int r2 = set_xattrs(a);
		if (r2 < ret) ret = r2;
	}

	/*
	 * Some flags prevent file modification; they must be restored after
	 * file contents are written.
	 */
	if (a->todo & TODO_FFLAGS) {
		int r2 = set_fflags(a);
		if (r2 < ret) ret = r2;
	}

	/*
	 * Time must follow most other metadata;
	 * otherwise atime will get changed.
	 */
	if (a->todo & TODO_TIMES) {
		int r2 = set_times_from_entry(a);
		if (r2 < ret) ret = r2;
	}

	/*
	 * Mac extended metadata includes ACLs.
	 */
	if (a->todo & TODO_MAC_METADATA) {
		const void *metadata;
		size_t metadata_size;
		metadata = archive_entry_mac_metadata(a->entry, &metadata_size);
		if (metadata != NULL && metadata_size > 0) {
			int r2 = set_mac_metadata(a, archive_entry_pathname(
			    a->entry), metadata, metadata_size);
			if (r2 < ret) ret = r2;
		}
	}

	/*
	 * ACLs must be restored after timestamps because there are
	 * ACLs that prevent attribute changes (including time).
	 */
	if (a->todo & TODO_ACLS) {
		int r2 = archive_write_disk_set_acls(&a->archive, a->fd,
				  archive_entry_pathname(a->entry),
				  archive_entry_acl(a->entry));
		if (r2 < ret) ret = r2;
	}

finish_metadata:
	/* If there's an fd, we can close it now. */
	if (a->fd >= 0) {
		close(a->fd);
		a->fd = -1;
	}
	/* If there's an entry, we can release it now. */
	if (a->entry) {
		archive_entry_free(a->entry);
		a->entry = NULL;
	}
	a->archive.state = ARCHIVE_STATE_HEADER;
	return (ret);
}

int
archive_write_disk_set_group_lookup(struct archive *_a,
    void *private_data,
    int64_t (*lookup_gid)(void *private, const char *gname, int64_t gid),
    void (*cleanup_gid)(void *private))
{
	struct archive_write_disk *a = (struct archive_write_disk *)_a;
	archive_check_magic(&a->archive, ARCHIVE_WRITE_DISK_MAGIC,
	    ARCHIVE_STATE_ANY, "archive_write_disk_set_group_lookup");

	if (a->cleanup_gid != NULL && a->lookup_gid_data != NULL)
		(a->cleanup_gid)(a->lookup_gid_data);

	a->lookup_gid = lookup_gid;
	a->cleanup_gid = cleanup_gid;
	a->lookup_gid_data = private_data;
	return (ARCHIVE_OK);
}

int
archive_write_disk_set_user_lookup(struct archive *_a,
    void *private_data,
    int64_t (*lookup_uid)(void *private, const char *uname, int64_t uid),
    void (*cleanup_uid)(void *private))
{
	struct archive_write_disk *a = (struct archive_write_disk *)_a;
	archive_check_magic(&a->archive, ARCHIVE_WRITE_DISK_MAGIC,
	    ARCHIVE_STATE_ANY, "archive_write_disk_set_user_lookup");

	if (a->cleanup_uid != NULL && a->lookup_uid_data != NULL)
		(a->cleanup_uid)(a->lookup_uid_data);

	a->lookup_uid = lookup_uid;
	a->cleanup_uid = cleanup_uid;
	a->lookup_uid_data = private_data;
	return (ARCHIVE_OK);
}

int64_t
archive_write_disk_gid(struct archive *_a, const char *name, int64_t id)
{
       struct archive_write_disk *a = (struct archive_write_disk *)_a;
       archive_check_magic(&a->archive, ARCHIVE_WRITE_DISK_MAGIC,
           ARCHIVE_STATE_ANY, "archive_write_disk_gid");
       if (a->lookup_gid)
               return (a->lookup_gid)(a->lookup_gid_data, name, id);
       return (id);
}
 
int64_t
archive_write_disk_uid(struct archive *_a, const char *name, int64_t id)
{
	struct archive_write_disk *a = (struct archive_write_disk *)_a;
	archive_check_magic(&a->archive, ARCHIVE_WRITE_DISK_MAGIC,
	    ARCHIVE_STATE_ANY, "archive_write_disk_uid");
	if (a->lookup_uid)
		return (a->lookup_uid)(a->lookup_uid_data, name, id);
	return (id);
}

/*
 * Create a new archive_write_disk object and initialize it with global state.
 */
struct archive *
archive_write_disk_new(void)
{
	struct archive_write_disk *a;

	a = (struct archive_write_disk *)malloc(sizeof(*a));
	if (a == NULL)
		return (NULL);
	memset(a, 0, sizeof(*a));
	a->archive.magic = ARCHIVE_WRITE_DISK_MAGIC;
	/* We're ready to write a header immediately. */
	a->archive.state = ARCHIVE_STATE_HEADER;
	a->archive.vtable = archive_write_disk_vtable();
	a->start_time = time(NULL);
	/* Query and restore the umask. */
	umask(a->user_umask = umask(0));
#ifdef HAVE_GETEUID
	a->user_uid = geteuid();
#endif /* HAVE_GETEUID */
	if (archive_string_ensure(&a->path_safe, 512) == NULL) {
		free(a);
		return (NULL);
	}
#ifdef HAVE_ZLIB_H
	a->decmpfs_compression_level = 5;
#endif
	return (&a->archive);
}


/*
 * If pathname is longer than PATH_MAX, chdir to a suitable
 * intermediate dir and edit the path down to a shorter suffix.  Note
 * that this routine never returns an error; if the chdir() attempt
 * fails for any reason, we just go ahead with the long pathname.  The
 * object creation is likely to fail, but any error will get handled
 * at that time.
 */
#if defined(HAVE_FCHDIR) && defined(PATH_MAX)
static void
edit_deep_directories(struct archive_write_disk *a)
{
	int ret;
	char *tail = a->name;

	/* If path is short, avoid the open() below. */
	if (strlen(tail) <= PATH_MAX)
		return;

	/* Try to record our starting dir. */
	a->restore_pwd = open(".", O_RDONLY | O_BINARY | O_CLOEXEC);
	__archive_ensure_cloexec_flag(a->restore_pwd);
	if (a->restore_pwd < 0)
		return;

	/* As long as the path is too long... */
	while (strlen(tail) > PATH_MAX) {
		/* Locate a dir prefix shorter than PATH_MAX. */
		tail += PATH_MAX - 8;
		while (tail > a->name && *tail != '/')
			tail--;
		/* Exit if we find a too-long path component. */
		if (tail <= a->name)
			return;
		/* Create the intermediate dir and chdir to it. */
		*tail = '\0'; /* Terminate dir portion */
		ret = create_dir(a, a->name);
		if (ret == ARCHIVE_OK && chdir(a->name) != 0)
			ret = ARCHIVE_FAILED;
		*tail = '/'; /* Restore the / we removed. */
		if (ret != ARCHIVE_OK)
			return;
		tail++;
		/* The chdir() succeeded; we've now shortened the path. */
		a->name = tail;
	}
	return;
}
#endif

/*
 * The main restore function.
 */
static int
restore_entry(struct archive_write_disk *a)
{
	int ret = ARCHIVE_OK, en;

	if (a->flags & ARCHIVE_EXTRACT_UNLINK && !S_ISDIR(a->mode)) {
		/*
		 * TODO: Fix this.  Apparently, there are platforms
		 * that still allow root to hose the entire filesystem
		 * by unlinking a dir.  The S_ISDIR() test above
		 * prevents us from using unlink() here if the new
		 * object is a dir, but that doesn't mean the old
		 * object isn't a dir.
		 */
		if (unlink(a->name) == 0) {
			/* We removed it, reset cached stat. */
			a->pst = NULL;
		} else if (errno == ENOENT) {
			/* File didn't exist, that's just as good. */
		} else if (rmdir(a->name) == 0) {
			/* It was a dir, but now it's gone. */
			a->pst = NULL;
		} else {
			/* We tried, but couldn't get rid of it. */
			archive_set_error(&a->archive, errno,
			    "Could not unlink");
			return(ARCHIVE_FAILED);
		}
	}

	/* Try creating it first; if this fails, we'll try to recover. */
	en = create_filesystem_object(a);

	if ((en == ENOTDIR || en == ENOENT)
	    && !(a->flags & ARCHIVE_EXTRACT_NO_AUTODIR)) {
		/* If the parent dir doesn't exist, try creating it. */
		create_parent_dir(a, a->name);
		/* Now try to create the object again. */
		en = create_filesystem_object(a);
	}

	if ((en == EISDIR || en == EEXIST)
	    && (a->flags & ARCHIVE_EXTRACT_NO_OVERWRITE)) {
		/* If we're not overwriting, we're done. */
		archive_entry_unset_size(a->entry);
		return (ARCHIVE_OK);
	}

	/*
	 * Some platforms return EISDIR if you call
	 * open(O_WRONLY | O_EXCL | O_CREAT) on a directory, some
	 * return EEXIST.  POSIX is ambiguous, requiring EISDIR
	 * for open(O_WRONLY) on a dir and EEXIST for open(O_EXCL | O_CREAT)
	 * on an existing item.
	 */
	if (en == EISDIR) {
		/* A dir is in the way of a non-dir, rmdir it. */
		if (rmdir(a->name) != 0) {
			archive_set_error(&a->archive, errno,
			    "Can't remove already-existing dir");
			return (ARCHIVE_FAILED);
		}
		a->pst = NULL;
		/* Try again. */
		en = create_filesystem_object(a);
	} else if (en == EEXIST) {
		/*
		 * We know something is in the way, but we don't know what;
		 * we need to find out before we go any further.
		 */
		int r = 0;
		/*
		 * The SECURE_SYMLINKS logic has already removed a
		 * symlink to a dir if the client wants that.  So
		 * follow the symlink if we're creating a dir.
		 */
		if (S_ISDIR(a->mode))
			r = stat(a->name, &a->st);
		/*
		 * If it's not a dir (or it's a broken symlink),
		 * then don't follow it.
		 */
		if (r != 0 || !S_ISDIR(a->mode))
			r = lstat(a->name, &a->st);
		if (r != 0) {
			archive_set_error(&a->archive, errno,
			    "Can't stat existing object");
			return (ARCHIVE_FAILED);
		}

		/*
		 * NO_OVERWRITE_NEWER doesn't apply to directories.
		 */
		if ((a->flags & ARCHIVE_EXTRACT_NO_OVERWRITE_NEWER)
		    &&  !S_ISDIR(a->st.st_mode)) {
			if (!older(&(a->st), a->entry)) {
				archive_entry_unset_size(a->entry);
				return (ARCHIVE_OK);
			}
		}

		/* If it's our archive, we're done. */
		if (a->skip_file_set &&
		    a->st.st_dev == (dev_t)a->skip_file_dev &&
		    a->st.st_ino == (ino_t)a->skip_file_ino) {
			archive_set_error(&a->archive, 0,
			    "Refusing to overwrite archive");
			return (ARCHIVE_FAILED);
		}

		if (!S_ISDIR(a->st.st_mode)) {
			/* A non-dir is in the way, unlink it. */
			if (unlink(a->name) != 0) {
				archive_set_error(&a->archive, errno,
				    "Can't unlink already-existing object");
				return (ARCHIVE_FAILED);
			}
			a->pst = NULL;
			/* Try again. */
			en = create_filesystem_object(a);
		} else if (!S_ISDIR(a->mode)) {
			/* A dir is in the way of a non-dir, rmdir it. */
			if (rmdir(a->name) != 0) {
				archive_set_error(&a->archive, errno,
				    "Can't replace existing directory with non-directory");
				return (ARCHIVE_FAILED);
			}
			/* Try again. */
			en = create_filesystem_object(a);
		} else {
			/*
			 * There's a dir in the way of a dir.  Don't
			 * waste time with rmdir()/mkdir(), just fix
			 * up the permissions on the existing dir.
			 * Note that we don't change perms on existing
			 * dirs unless _EXTRACT_PERM is specified.
			 */
			if ((a->mode != a->st.st_mode)
			    && (a->todo & TODO_MODE_FORCE))
				a->deferred |= (a->todo & TODO_MODE);
			/* Ownership doesn't need deferred fixup. */
			en = 0; /* Forget the EEXIST. */
		}
	}

	if (en) {
		/* Everything failed; give up here. */
		archive_set_error(&a->archive, en, "Can't create '%s'",
		    a->name);
		return (ARCHIVE_FAILED);
	}

	a->pst = NULL; /* Cached stat data no longer valid. */
	return (ret);
}

/*
 * Returns 0 if creation succeeds, or else returns errno value from
 * the failed system call.   Note:  This function should only ever perform
 * a single system call.
 */
static int
create_filesystem_object(struct archive_write_disk *a)
{
	/* Create the entry. */
	const char *linkname;
	mode_t final_mode, mode;
	int r;

	/* We identify hard/symlinks according to the link names. */
	/* Since link(2) and symlink(2) don't handle modes, we're done here. */
	linkname = archive_entry_hardlink(a->entry);
	if (linkname != NULL) {
#if !HAVE_LINK
		return (EPERM);
#else
		r = link(linkname, a->name) ? errno : 0;
		/*
		 * New cpio and pax formats allow hardlink entries
		 * to carry data, so we may have to open the file
		 * for hardlink entries.
		 *
		 * If the hardlink was successfully created and
		 * the archive doesn't have carry data for it,
		 * consider it to be non-authoritative for meta data.
		 * This is consistent with GNU tar and BSD pax.
		 * If the hardlink does carry data, let the last
		 * archive entry decide ownership.
		 */
		if (r == 0 && a->filesize <= 0) {
			a->todo = 0;
			a->deferred = 0;
		} else if (r == 0 && a->filesize > 0) {
			a->fd = open(a->name,
				     O_WRONLY | O_TRUNC | O_BINARY | O_CLOEXEC);
			__archive_ensure_cloexec_flag(a->fd);
			if (a->fd < 0)
				r = errno;
		}
		return (r);
#endif
	}
	linkname = archive_entry_symlink(a->entry);
	if (linkname != NULL) {
#if HAVE_SYMLINK
		return symlink(linkname, a->name) ? errno : 0;
#else
		return (EPERM);
#endif
	}

	/*
	 * The remaining system calls all set permissions, so let's
	 * try to take advantage of that to avoid an extra chmod()
	 * call.  (Recall that umask is set to zero right now!)
	 */

	/* Mode we want for the final restored object (w/o file type bits). */
	final_mode = a->mode & 07777;
	/*
	 * The mode that will actually be restored in this step.  Note
	 * that SUID, SGID, etc, require additional work to ensure
	 * security, so we never restore them at this point.
	 */
	mode = final_mode & 0777 & ~a->user_umask;

	switch (a->mode & AE_IFMT) {
	default:
		/* POSIX requires that we fall through here. */
		/* FALLTHROUGH */
	case AE_IFREG:
		a->fd = open(a->name,
		    O_WRONLY | O_CREAT | O_EXCL | O_BINARY | O_CLOEXEC, mode);
		__archive_ensure_cloexec_flag(a->fd);
		r = (a->fd < 0);
		break;
	case AE_IFCHR:
#ifdef HAVE_MKNOD
		/* Note: we use AE_IFCHR for the case label, and
		 * S_IFCHR for the mknod() call.  This is correct.  */
		r = mknod(a->name, mode | S_IFCHR,
		    archive_entry_rdev(a->entry));
		break;
#else
		/* TODO: Find a better way to warn about our inability
		 * to restore a char device node. */
		return (EINVAL);
#endif /* HAVE_MKNOD */
	case AE_IFBLK:
#ifdef HAVE_MKNOD
		r = mknod(a->name, mode | S_IFBLK,
		    archive_entry_rdev(a->entry));
		break;
#else
		/* TODO: Find a better way to warn about our inability
		 * to restore a block device node. */
		return (EINVAL);
#endif /* HAVE_MKNOD */
	case AE_IFDIR:
		mode = (mode | MINIMUM_DIR_MODE) & MAXIMUM_DIR_MODE;
		r = mkdir(a->name, mode);
		if (r == 0) {
			/* Defer setting dir times. */
			a->deferred |= (a->todo & TODO_TIMES);
			a->todo &= ~TODO_TIMES;
			/* Never use an immediate chmod(). */
			/* We can't avoid the chmod() entirely if EXTRACT_PERM
			 * because of SysV SGID inheritance. */
			if ((mode != final_mode)
			    || (a->flags & ARCHIVE_EXTRACT_PERM))
				a->deferred |= (a->todo & TODO_MODE);
			a->todo &= ~TODO_MODE;
		}
		break;
	case AE_IFIFO:
#ifdef HAVE_MKFIFO
		r = mkfifo(a->name, mode);
		break;
#else
		/* TODO: Find a better way to warn about our inability
		 * to restore a fifo. */
		return (EINVAL);
#endif /* HAVE_MKFIFO */
	}

	/* All the system calls above set errno on failure. */
	if (r)
		return (errno);

	/* If we managed to set the final mode, we've avoided a chmod(). */
	if (mode == final_mode)
		a->todo &= ~TODO_MODE;
	return (0);
}

/*
 * Cleanup function for archive_extract.  Mostly, this involves processing
 * the fixup list, which is used to address a number of problems:
 *   * Dir permissions might prevent us from restoring a file in that
 *     dir, so we restore the dir with minimum 0700 permissions first,
 *     then correct the mode at the end.
 *   * Similarly, the act of restoring a file touches the directory
 *     and changes the timestamp on the dir, so we have to touch-up dir
 *     timestamps at the end as well.
 *   * Some file flags can interfere with the restore by, for example,
 *     preventing the creation of hardlinks to those files.
 *   * Mac OS extended metadata includes ACLs, so must be deferred on dirs.
 *
 * Note that tar/cpio do not require that archives be in a particular
 * order; there is no way to know when the last file has been restored
 * within a directory, so there's no way to optimize the memory usage
 * here by fixing up the directory any earlier than the
 * end-of-archive.
 *
 * XXX TODO: Directory ACLs should be restored here, for the same
 * reason we set directory perms here. XXX
 */
static int
_archive_write_disk_close(struct archive *_a)
{
	struct archive_write_disk *a = (struct archive_write_disk *)_a;
	struct fixup_entry *next, *p;
	int ret;

	archive_check_magic(&a->archive, ARCHIVE_WRITE_DISK_MAGIC,
	    ARCHIVE_STATE_HEADER | ARCHIVE_STATE_DATA,
	    "archive_write_disk_close");
	ret = _archive_write_disk_finish_entry(&a->archive);

	/* Sort dir list so directories are fixed up in depth-first order. */
	p = sort_dir_list(a->fixup_list);

	while (p != NULL) {
		a->pst = NULL; /* Mark stat cache as out-of-date. */
		if (p->fixup & TODO_TIMES) {
			set_times(a, -1, p->mode, p->name,
			    p->atime, p->atime_nanos,
			    p->birthtime, p->birthtime_nanos,
			    p->mtime, p->mtime_nanos,
			    p->ctime, p->ctime_nanos);
		}
		if (p->fixup & TODO_MODE_BASE)
			chmod(p->name, p->mode);
		if (p->fixup & TODO_ACLS)
			archive_write_disk_set_acls(&a->archive,
						    -1, p->name, &p->acl);
		if (p->fixup & TODO_FFLAGS)
			set_fflags_platform(a, -1, p->name,
			    p->mode, p->fflags_set, 0);
		if (p->fixup & TODO_MAC_METADATA)
			set_mac_metadata(a, p->name, p->mac_metadata,
					 p->mac_metadata_size);
		next = p->next;
		archive_acl_clear(&p->acl);
		free(p->mac_metadata);
		free(p->name);
		free(p);
		p = next;
	}
	a->fixup_list = NULL;
	return (ret);
}

static int
_archive_write_disk_free(struct archive *_a)
{
	struct archive_write_disk *a;
	int ret;
	if (_a == NULL)
		return (ARCHIVE_OK);
	archive_check_magic(_a, ARCHIVE_WRITE_DISK_MAGIC,
	    ARCHIVE_STATE_ANY | ARCHIVE_STATE_FATAL, "archive_write_disk_free");
	a = (struct archive_write_disk *)_a;
	ret = _archive_write_disk_close(&a->archive);
	archive_write_disk_set_group_lookup(&a->archive, NULL, NULL, NULL);
	archive_write_disk_set_user_lookup(&a->archive, NULL, NULL, NULL);
	if (a->entry)
		archive_entry_free(a->entry);
	archive_string_free(&a->_name_data);
	archive_string_free(&a->archive.error_string);
	archive_string_free(&a->path_safe);
	a->archive.magic = 0;
	__archive_clean(&a->archive);
	free(a->decmpfs_header_p);
	free(a->resource_fork);
	free(a->compressed_buffer);
	free(a->uncompressed_buffer);
#ifdef HAVE_ZLIB_H
	if (a->stream_valid) {
		switch (deflateEnd(&a->stream)) {
		case Z_OK:
			break;
		default:
			archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
			    "Failed to clean up compressor");
			ret = ARCHIVE_FATAL;
			break;
		}
	}
#endif
	free(a);
	return (ret);
}

/*
 * Simple O(n log n) merge sort to order the fixup list.  In
 * particular, we want to restore dir timestamps depth-first.
 */
static struct fixup_entry *
sort_dir_list(struct fixup_entry *p)
{
	struct fixup_entry *a, *b, *t;

	if (p == NULL)
		return (NULL);
	/* A one-item list is already sorted. */
	if (p->next == NULL)
		return (p);

	/* Step 1: split the list. */
	t = p;
	a = p->next->next;
	while (a != NULL) {
		/* Step a twice, t once. */
		a = a->next;
		if (a != NULL)
			a = a->next;
		t = t->next;
	}
	/* Now, t is at the mid-point, so break the list here. */
	b = t->next;
	t->next = NULL;
	a = p;

	/* Step 2: Recursively sort the two sub-lists. */
	a = sort_dir_list(a);
	b = sort_dir_list(b);

	/* Step 3: Merge the returned lists. */
	/* Pick the first element for the merged list. */
	if (strcmp(a->name, b->name) > 0) {
		t = p = a;
		a = a->next;
	} else {
		t = p = b;
		b = b->next;
	}

	/* Always put the later element on the list first. */
	while (a != NULL && b != NULL) {
		if (strcmp(a->name, b->name) > 0) {
			t->next = a;
			a = a->next;
		} else {
			t->next = b;
			b = b->next;
		}
		t = t->next;
	}

	/* Only one list is non-empty, so just splice it on. */
	if (a != NULL)
		t->next = a;
	if (b != NULL)
		t->next = b;

	return (p);
}

/*
 * Returns a new, initialized fixup entry.
 *
 * TODO: Reduce the memory requirements for this list by using a tree
 * structure rather than a simple list of names.
 */
static struct fixup_entry *
new_fixup(struct archive_write_disk *a, const char *pathname)
{
	struct fixup_entry *fe;

	fe = (struct fixup_entry *)calloc(1, sizeof(struct fixup_entry));
	if (fe == NULL) {
		archive_set_error(&a->archive, ENOMEM,
		    "Can't allocate memory for a fixup");
		return (NULL);
	}
	fe->next = a->fixup_list;
	a->fixup_list = fe;
	fe->fixup = 0;
	fe->name = strdup(pathname);
	return (fe);
}

/*
 * Returns a fixup structure for the current entry.
 */
static struct fixup_entry *
current_fixup(struct archive_write_disk *a, const char *pathname)
{
	if (a->current_fixup == NULL)
		a->current_fixup = new_fixup(a, pathname);
	return (a->current_fixup);
}

/* TODO: Make this work. */
/*
 * TODO: The deep-directory support bypasses this; disable deep directory
 * support if we're doing symlink checks.
 */
/*
 * TODO: Someday, integrate this with the deep dir support; they both
 * scan the path and both can be optimized by comparing against other
 * recent paths.
 */
/* TODO: Extend this to support symlinks on Windows Vista and later. */
static int
check_symlinks(struct archive_write_disk *a)
{
#if !defined(HAVE_LSTAT)
	/* Platform doesn't have lstat, so we can't look for symlinks. */
	(void)a; /* UNUSED */
	return (ARCHIVE_OK);
#else
	char *pn;
	char c;
	int r;
	struct stat st;

	/*
	 * Guard against symlink tricks.  Reject any archive entry whose
	 * destination would be altered by a symlink.
	 */
	/* Whatever we checked last time doesn't need to be re-checked. */
	pn = a->name;
	if (archive_strlen(&(a->path_safe)) > 0) {
		char *p = a->path_safe.s;
		while ((*pn != '\0') && (*p == *pn))
			++p, ++pn;
	}
	c = pn[0];
	/* Keep going until we've checked the entire name. */
	while (pn[0] != '\0' && (pn[0] != '/' || pn[1] != '\0')) {
		/* Skip the next path element. */
		while (*pn != '\0' && *pn != '/')
			++pn;
		c = pn[0];
		pn[0] = '\0';
		/* Check that we haven't hit a symlink. */
		r = lstat(a->name, &st);
		if (r != 0) {
			/* We've hit a dir that doesn't exist; stop now. */
			if (errno == ENOENT)
				break;
		} else if (S_ISLNK(st.st_mode)) {
			if (c == '\0') {
				/*
				 * Last element is symlink; remove it
				 * so we can overwrite it with the
				 * item being extracted.
				 */
				if (unlink(a->name)) {
					archive_set_error(&a->archive, errno,
					    "Could not remove symlink %s",
					    a->name);
					pn[0] = c;
					return (ARCHIVE_FAILED);
				}
				a->pst = NULL;
				/*
				 * Even if we did remove it, a warning
				 * is in order.  The warning is silly,
				 * though, if we're just replacing one
				 * symlink with another symlink.
				 */
				if (!S_ISLNK(a->mode)) {
					archive_set_error(&a->archive, 0,
					    "Removing symlink %s",
					    a->name);
				}
				/* Symlink gone.  No more problem! */
				pn[0] = c;
				return (0);
			} else if (a->flags & ARCHIVE_EXTRACT_UNLINK) {
				/* User asked us to remove problems. */
				if (unlink(a->name) != 0) {
					archive_set_error(&a->archive, 0,
					    "Cannot remove intervening symlink %s",
					    a->name);
					pn[0] = c;
					return (ARCHIVE_FAILED);
				}
				a->pst = NULL;
			} else {
				archive_set_error(&a->archive, 0,
				    "Cannot extract through symlink %s",
				    a->name);
				pn[0] = c;
				return (ARCHIVE_FAILED);
			}
		}
	}
	pn[0] = c;
	/* We've checked and/or cleaned the whole path, so remember it. */
	archive_strcpy(&a->path_safe, a->name);
	return (ARCHIVE_OK);
#endif
}

#if defined(__CYGWIN__)
/*
 * 1. Convert a path separator from '\' to '/' .
 *    We shouldn't check multibyte character directly because some
 *    character-set have been using the '\' character for a part of
 *    its multibyte character code.
 * 2. Replace unusable characters in Windows with underscore('_').
 * See also : http://msdn.microsoft.com/en-us/library/aa365247.aspx
 */
static void
cleanup_pathname_win(struct archive_write_disk *a)
{
	wchar_t wc;
	char *p;
	size_t alen, l;
	int mb, complete, utf8;

	alen = 0;
	mb = 0;
	complete = 1;
	utf8 = (strcmp(nl_langinfo(CODESET), "UTF-8") == 0)? 1: 0;
	for (p = a->name; *p != '\0'; p++) {
		++alen;
		if (*p == '\\') {
			/* If previous byte is smaller than 128,
			 * this is not second byte of multibyte characters,
			 * so we can replace '\' with '/'. */
			if (utf8 || !mb)
				*p = '/';
			else
				complete = 0;/* uncompleted. */
		} else if (*(unsigned char *)p > 127)
			mb = 1;
		else
			mb = 0;
		/* Rewrite the path name if its next character is unusable. */
		if (*p == ':' || *p == '*' || *p == '?' || *p == '"' ||
		    *p == '<' || *p == '>' || *p == '|')
			*p = '_';
	}
	if (complete)
		return;

	/*
	 * Convert path separator in wide-character.
	 */
	p = a->name;
	while (*p != '\0' && alen) {
		l = mbtowc(&wc, p, alen);
		if (l == (size_t)-1) {
			while (*p != '\0') {
				if (*p == '\\')
					*p = '/';
				++p;
			}
			break;
		}
		if (l == 1 && wc == L'\\')
			*p = '/';
		p += l;
		alen -= l;
	}
}
#endif

/*
 * Canonicalize the pathname.  In particular, this strips duplicate
 * '/' characters, '.' elements, and trailing '/'.  It also raises an
 * error for an empty path, a trailing '..' or (if _SECURE_NODOTDOT is
 * set) any '..' in the path.
 */
static int
cleanup_pathname(struct archive_write_disk *a)
{
	char *dest, *src;
	char separator = '\0';

	dest = src = a->name;
	if (*src == '\0') {
		archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
		    "Invalid empty pathname");
		return (ARCHIVE_FAILED);
	}

#if defined(__CYGWIN__)
	cleanup_pathname_win(a);
#endif
	/* Skip leading '/'. */
	if (*src == '/')
		separator = *src++;

	/* Scan the pathname one element at a time. */
	for (;;) {
		/* src points to first char after '/' */
		if (src[0] == '\0') {
			break;
		} else if (src[0] == '/') {
			/* Found '//', ignore second one. */
			src++;
			continue;
		} else if (src[0] == '.') {
			if (src[1] == '\0') {
				/* Ignore trailing '.' */
				break;
			} else if (src[1] == '/') {
				/* Skip './'. */
				src += 2;
				continue;
			} else if (src[1] == '.') {
				if (src[2] == '/' || src[2] == '\0') {
					/* Conditionally warn about '..' */
					if (a->flags & ARCHIVE_EXTRACT_SECURE_NODOTDOT) {
						archive_set_error(&a->archive,
						    ARCHIVE_ERRNO_MISC,
						    "Path contains '..'");
						return (ARCHIVE_FAILED);
					}
				}
				/*
				 * Note: Under no circumstances do we
				 * remove '..' elements.  In
				 * particular, restoring
				 * '/foo/../bar/' should create the
				 * 'foo' dir as a side-effect.
				 */
			}
		}

		/* Copy current element, including leading '/'. */
		if (separator)
			*dest++ = '/';
		while (*src != '\0' && *src != '/') {
			*dest++ = *src++;
		}

		if (*src == '\0')
			break;

		/* Skip '/' separator. */
		separator = *src++;
	}
	/*
	 * We've just copied zero or more path elements, not including the
	 * final '/'.
	 */
	if (dest == a->name) {
		/*
		 * Nothing got copied.  The path must have been something
		 * like '.' or '/' or './' or '/././././/./'.
		 */
		if (separator)
			*dest++ = '/';
		else
			*dest++ = '.';
	}
	/* Terminate the result. */
	*dest = '\0';
	return (ARCHIVE_OK);
}

/*
 * Create the parent directory of the specified path, assuming path
 * is already in mutable storage.
 */
static int
create_parent_dir(struct archive_write_disk *a, char *path)
{
	char *slash;
	int r;

	/* Remove tail element to obtain parent name. */
	slash = strrchr(path, '/');
	if (slash == NULL)
		return (ARCHIVE_OK);
	*slash = '\0';
	r = create_dir(a, path);
	*slash = '/';
	return (r);
}

/*
 * Create the specified dir, recursing to create parents as necessary.
 *
 * Returns ARCHIVE_OK if the path exists when we're done here.
 * Otherwise, returns ARCHIVE_FAILED.
 * Assumes path is in mutable storage; path is unchanged on exit.
 */
static int
create_dir(struct archive_write_disk *a, char *path)
{
	struct stat st;
	struct fixup_entry *le;
	char *slash, *base;
	mode_t mode_final, mode;
	int r;

	/* Check for special names and just skip them. */
	slash = strrchr(path, '/');
	if (slash == NULL)
		base = path;
	else
		base = slash + 1;

	if (base[0] == '\0' ||
	    (base[0] == '.' && base[1] == '\0') ||
	    (base[0] == '.' && base[1] == '.' && base[2] == '\0')) {
		/* Don't bother trying to create null path, '.', or '..'. */
		if (slash != NULL) {
			*slash = '\0';
			r = create_dir(a, path);
			*slash = '/';
			return (r);
		}
		return (ARCHIVE_OK);
	}

	/*
	 * Yes, this should be stat() and not lstat().  Using lstat()
	 * here loses the ability to extract through symlinks.  Also note
	 * that this should not use the a->st cache.
	 */
	if (stat(path, &st) == 0) {
		if (S_ISDIR(st.st_mode))
			return (ARCHIVE_OK);
		if ((a->flags & ARCHIVE_EXTRACT_NO_OVERWRITE)) {
			archive_set_error(&a->archive, EEXIST,
			    "Can't create directory '%s'", path);
			return (ARCHIVE_FAILED);
		}
		if (unlink(path) != 0) {
			archive_set_error(&a->archive, errno,
			    "Can't create directory '%s': "
			    "Conflicting file cannot be removed",
			    path);
			return (ARCHIVE_FAILED);
		}
	} else if (errno != ENOENT && errno != ENOTDIR) {
		/* Stat failed? */
		archive_set_error(&a->archive, errno, "Can't test directory '%s'", path);
		return (ARCHIVE_FAILED);
	} else if (slash != NULL) {
		*slash = '\0';
		r = create_dir(a, path);
		*slash = '/';
		if (r != ARCHIVE_OK)
			return (r);
	}

	/*
	 * Mode we want for the final restored directory.  Per POSIX,
	 * implicitly-created dirs must be created obeying the umask.
	 * There's no mention whether this is different for privileged
	 * restores (which the rest of this code handles by pretending
	 * umask=0).  I've chosen here to always obey the user's umask for
	 * implicit dirs, even if _EXTRACT_PERM was specified.
	 */
	mode_final = DEFAULT_DIR_MODE & ~a->user_umask;
	/* Mode we want on disk during the restore process. */
	mode = mode_final;
	mode |= MINIMUM_DIR_MODE;
	mode &= MAXIMUM_DIR_MODE;
	if (mkdir(path, mode) == 0) {
		if (mode != mode_final) {
			le = new_fixup(a, path);
			if (le == NULL)
				return (ARCHIVE_FATAL);
			le->fixup |=TODO_MODE_BASE;
			le->mode = mode_final;
		}
		return (ARCHIVE_OK);
	}

	/*
	 * Without the following check, a/b/../b/c/d fails at the
	 * second visit to 'b', so 'd' can't be created.  Note that we
	 * don't add it to the fixup list here, as it's already been
	 * added.
	 */
	if (stat(path, &st) == 0 && S_ISDIR(st.st_mode))
		return (ARCHIVE_OK);

	archive_set_error(&a->archive, errno, "Failed to create dir '%s'",
	    path);
	return (ARCHIVE_FAILED);
}

/*
 * Note: Although we can skip setting the user id if the desired user
 * id matches the current user, we cannot skip setting the group, as
 * many systems set the gid based on the containing directory.  So
 * we have to perform a chown syscall if we want to set the SGID
 * bit.  (The alternative is to stat() and then possibly chown(); it's
 * more efficient to skip the stat() and just always chown().)  Note
 * that a successful chown() here clears the TODO_SGID_CHECK bit, which
 * allows set_mode to skip the stat() check for the GID.
 */
static int
set_ownership(struct archive_write_disk *a)
{
#ifndef __CYGWIN__
/* unfortunately, on win32 there is no 'root' user with uid 0,
   so we just have to try the chown and see if it works */

	/* If we know we can't change it, don't bother trying. */
	if (a->user_uid != 0  &&  a->user_uid != a->uid) {
		archive_set_error(&a->archive, errno,
		    "Can't set UID=%jd", (intmax_t)a->uid);
		return (ARCHIVE_WARN);
	}
#endif

#ifdef HAVE_FCHOWN
	/* If we have an fd, we can avoid a race. */
	if (a->fd >= 0 && fchown(a->fd, a->uid, a->gid) == 0) {
		/* We've set owner and know uid/gid are correct. */
		a->todo &= ~(TODO_OWNER | TODO_SGID_CHECK | TODO_SUID_CHECK);
		return (ARCHIVE_OK);
	}
#endif

	/* We prefer lchown() but will use chown() if that's all we have. */
	/* Of course, if we have neither, this will always fail. */
#ifdef HAVE_LCHOWN
	if (lchown(a->name, a->uid, a->gid) == 0) {
		/* We've set owner and know uid/gid are correct. */
		a->todo &= ~(TODO_OWNER | TODO_SGID_CHECK | TODO_SUID_CHECK);
		return (ARCHIVE_OK);
	}
#elif HAVE_CHOWN
	if (!S_ISLNK(a->mode) && chown(a->name, a->uid, a->gid) == 0) {
		/* We've set owner and know uid/gid are correct. */
		a->todo &= ~(TODO_OWNER | TODO_SGID_CHECK | TODO_SUID_CHECK);
		return (ARCHIVE_OK);
	}
#endif

	archive_set_error(&a->archive, errno,
	    "Can't set user=%jd/group=%jd for %s",
	    (intmax_t)a->uid, (intmax_t)a->gid, a->name);
	return (ARCHIVE_WARN);
}

/*
 * Note: Returns 0 on success, non-zero on failure.
 */
static int
set_time(int fd, int mode, const char *name,
    time_t atime, long atime_nsec,
    time_t mtime, long mtime_nsec)
{
	/* Select the best implementation for this platform. */
#if defined(HAVE_UTIMENSAT) && defined(HAVE_FUTIMENS)
	/*
	 * utimensat() and futimens() are defined in
	 * POSIX.1-2008. They support ns resolution and setting times
	 * on fds and symlinks.
	 */
	struct timespec ts[2];
	(void)mode; /* UNUSED */
	ts[0].tv_sec = atime;
	ts[0].tv_nsec = atime_nsec;
	ts[1].tv_sec = mtime;
	ts[1].tv_nsec = mtime_nsec;
	if (fd >= 0)
		return futimens(fd, ts);
	return utimensat(AT_FDCWD, name, ts, AT_SYMLINK_NOFOLLOW);

#elif HAVE_UTIMES
	/*
	 * The utimes()-family functions support µs-resolution and
	 * setting times fds and symlinks.  utimes() is documented as
	 * LEGACY by POSIX, futimes() and lutimes() are not described
	 * in POSIX.
	 */
	struct timeval times[2];

	times[0].tv_sec = atime;
	times[0].tv_usec = atime_nsec / 1000;
	times[1].tv_sec = mtime;
	times[1].tv_usec = mtime_nsec / 1000;

#ifdef HAVE_FUTIMES
	if (fd >= 0)
		return (futimes(fd, times));
#else
	(void)fd; /* UNUSED */
#endif
#ifdef HAVE_LUTIMES
	(void)mode; /* UNUSED */
	return (lutimes(name, times));
#else
	if (S_ISLNK(mode))
		return (0);
	return (utimes(name, times));
#endif

#elif defined(HAVE_UTIME)
	/*
	 * utime() is POSIX-standard but only supports 1s resolution and
	 * does not support fds or symlinks.
	 */
	struct utimbuf times;
	(void)fd; /* UNUSED */
	(void)name; /* UNUSED */
	(void)atime_nsec; /* UNUSED */
	(void)mtime_nsec; /* UNUSED */
	times.actime = atime;
	times.modtime = mtime;
	if (S_ISLNK(mode))
		return (ARCHIVE_OK);
	return (utime(name, &times));

#else
	/*
	 * We don't know how to set the time on this platform.
	 */
	(void)fd; /* UNUSED */
	(void)mode; /* UNUSED */
	(void)name; /* UNUSED */
	(void)atime_nsec; /* UNUSED */
	(void)mtime_nsec; /* UNUSED */
	return (ARCHIVE_WARN);
#endif
}

#ifdef F_SETTIMES /* Tru64 */
static int
set_time_tru64(int fd, int mode, const char *name,
    time_t atime, long atime_nsec,
    time_t mtime, long mtime_nsec,
    time_t ctime, long ctime_nsec)
{
	struct attr_timbuf tstamp;
	struct timeval times[3];
	times[0].tv_sec = atime;
	times[0].tv_usec = atime_nsec / 1000;
	times[1].tv_sec = mtime;
	times[1].tv_usec = mtime_nsec / 1000;
	times[2].tv_sec = ctime;
	times[2].tv_usec = ctime_nsec / 1000;
	tstamp.atime = times[0];
	tstamp.mtime = times[1];
	tstamp.ctime = times[2];
	return (fcntl(fd,F_SETTIMES,&tstamp));
}
#endif /* Tru64 */

static int
set_times(struct archive_write_disk *a,
    int fd, int mode, const char *name,
    time_t atime, long atime_nanos,
    time_t birthtime, long birthtime_nanos,
    time_t mtime, long mtime_nanos,
    time_t cctime, long ctime_nanos)
{
	/* Note: set_time doesn't use libarchive return conventions!
	 * It uses syscall conventions.  So 0 here instead of ARCHIVE_OK. */
	int r1 = 0, r2 = 0;

#ifdef F_SETTIMES
	 /*
	 * on Tru64 try own fcntl first which can restore even the
	 * ctime, fall back to default code path below if it fails
	 * or if we are not running as root
	 */
	if (a->user_uid == 0 &&
	    set_time_tru64(fd, mode, name,
			   atime, atime_nanos, mtime,
			   mtime_nanos, cctime, ctime_nanos) == 0) {
		return (ARCHIVE_OK);
	}
#else /* Tru64 */
	(void)cctime; /* UNUSED */
	(void)ctime_nanos; /* UNUSED */
#endif /* Tru64 */

#ifdef HAVE_STRUCT_STAT_ST_BIRTHTIME
	/*
	 * If you have struct stat.st_birthtime, we assume BSD
	 * birthtime semantics, in which {f,l,}utimes() updates
	 * birthtime to earliest mtime.  So we set the time twice,
	 * first using the birthtime, then using the mtime.  If
	 * birthtime == mtime, this isn't necessary, so we skip it.
	 * If birthtime > mtime, then this won't work, so we skip it.
	 */
	if (birthtime < mtime
	    || (birthtime == mtime && birthtime_nanos < mtime_nanos))
		r1 = set_time(fd, mode, name,
			      atime, atime_nanos,
			      birthtime, birthtime_nanos);
#else
	(void)birthtime; /* UNUSED */
	(void)birthtime_nanos; /* UNUSED */
#endif
	r2 = set_time(fd, mode, name,
		      atime, atime_nanos,
		      mtime, mtime_nanos);
	if (r1 != 0 || r2 != 0) {
		archive_set_error(&a->archive, errno,
				  "Can't restore time");
		return (ARCHIVE_WARN);
	}
	return (ARCHIVE_OK);
}

static int
set_times_from_entry(struct archive_write_disk *a)
{
	time_t atime, birthtime, mtime, cctime;
	long atime_nsec, birthtime_nsec, mtime_nsec, ctime_nsec;

	/* Suitable defaults. */
	atime = birthtime = mtime = cctime = a->start_time;
	atime_nsec = birthtime_nsec = mtime_nsec = ctime_nsec = 0;

	/* If no time was provided, we're done. */
	if (!archive_entry_atime_is_set(a->entry)
#if HAVE_STRUCT_STAT_ST_BIRTHTIME
	    && !archive_entry_birthtime_is_set(a->entry)
#endif
	    && !archive_entry_mtime_is_set(a->entry))
		return (ARCHIVE_OK);

	if (archive_entry_atime_is_set(a->entry)) {
		atime = archive_entry_atime(a->entry);
		atime_nsec = archive_entry_atime_nsec(a->entry);
	}
	if (archive_entry_birthtime_is_set(a->entry)) {
		birthtime = archive_entry_birthtime(a->entry);
		birthtime_nsec = archive_entry_birthtime_nsec(a->entry);
	}
	if (archive_entry_mtime_is_set(a->entry)) {
		mtime = archive_entry_mtime(a->entry);
		mtime_nsec = archive_entry_mtime_nsec(a->entry);
	}
	if (archive_entry_ctime_is_set(a->entry)) {
		cctime = archive_entry_ctime(a->entry);
		ctime_nsec = archive_entry_ctime_nsec(a->entry);
	}

	return set_times(a, a->fd, a->mode, a->name,
			 atime, atime_nsec,
			 birthtime, birthtime_nsec,
			 mtime, mtime_nsec,
			 cctime, ctime_nsec);
}

static int
set_mode(struct archive_write_disk *a, int mode)
{
	int r = ARCHIVE_OK;
	mode &= 07777; /* Strip off file type bits. */

	if (a->todo & TODO_SGID_CHECK) {
		/*
		 * If we don't know the GID is right, we must stat()
		 * to verify it.  We can't just check the GID of this
		 * process, since systems sometimes set GID from
		 * the enclosing dir or based on ACLs.
		 */
		if ((r = lazy_stat(a)) != ARCHIVE_OK)
			return (r);
		if (a->pst->st_gid != a->gid) {
			mode &= ~ S_ISGID;
			if (a->flags & ARCHIVE_EXTRACT_OWNER) {
				/*
				 * This is only an error if you
				 * requested owner restore.  If you
				 * didn't, we'll try to restore
				 * sgid/suid, but won't consider it a
				 * problem if we can't.
				 */
				archive_set_error(&a->archive, -1,
				    "Can't restore SGID bit");
				r = ARCHIVE_WARN;
			}
		}
		/* While we're here, double-check the UID. */
		if (a->pst->st_uid != a->uid
		    && (a->todo & TODO_SUID)) {
			mode &= ~ S_ISUID;
			if (a->flags & ARCHIVE_EXTRACT_OWNER) {
				archive_set_error(&a->archive, -1,
				    "Can't restore SUID bit");
				r = ARCHIVE_WARN;
			}
		}
		a->todo &= ~TODO_SGID_CHECK;
		a->todo &= ~TODO_SUID_CHECK;
	} else if (a->todo & TODO_SUID_CHECK) {
		/*
		 * If we don't know the UID is right, we can just check
		 * the user, since all systems set the file UID from
		 * the process UID.
		 */
		if (a->user_uid != a->uid) {
			mode &= ~ S_ISUID;
			if (a->flags & ARCHIVE_EXTRACT_OWNER) {
				archive_set_error(&a->archive, -1,
				    "Can't make file SUID");
				r = ARCHIVE_WARN;
			}
		}
		a->todo &= ~TODO_SUID_CHECK;
	}

	if (S_ISLNK(a->mode)) {
#ifdef HAVE_LCHMOD
		/*
		 * If this is a symlink, use lchmod().  If the
		 * platform doesn't support lchmod(), just skip it.  A
		 * platform that doesn't provide a way to set
		 * permissions on symlinks probably ignores
		 * permissions on symlinks, so a failure here has no
		 * impact.
		 */
		if (lchmod(a->name, mode) != 0) {
			archive_set_error(&a->archive, errno,
			    "Can't set permissions to 0%o", (int)mode);
			r = ARCHIVE_WARN;
		}
#endif
	} else if (!S_ISDIR(a->mode)) {
		/*
		 * If it's not a symlink and not a dir, then use
		 * fchmod() or chmod(), depending on whether we have
		 * an fd.  Dirs get their perms set during the
		 * post-extract fixup, which is handled elsewhere.
		 */
#ifdef HAVE_FCHMOD
		if (a->fd >= 0) {
			if (fchmod(a->fd, mode) != 0) {
				archive_set_error(&a->archive, errno,
				    "Can't set permissions to 0%o", (int)mode);
				r = ARCHIVE_WARN;
			}
		} else
#endif
			/* If this platform lacks fchmod(), then
			 * we'll just use chmod(). */
			if (chmod(a->name, mode) != 0) {
				archive_set_error(&a->archive, errno,
				    "Can't set permissions to 0%o", (int)mode);
				r = ARCHIVE_WARN;
			}
	}
	return (r);
}

static int
set_fflags(struct archive_write_disk *a)
{
	struct fixup_entry *le;
	unsigned long	set, clear;
	int		r;
	int		critical_flags;
	mode_t		mode = archive_entry_mode(a->entry);

	/*
	 * Make 'critical_flags' hold all file flags that can't be
	 * immediately restored.  For example, on BSD systems,
	 * SF_IMMUTABLE prevents hardlinks from being created, so
	 * should not be set until after any hardlinks are created.  To
	 * preserve some semblance of portability, this uses #ifdef
	 * extensively.  Ugly, but it works.
	 *
	 * Yes, Virginia, this does create a security race.  It's mitigated
	 * somewhat by the practice of creating dirs 0700 until the extract
	 * is done, but it would be nice if we could do more than that.
	 * People restoring critical file systems should be wary of
	 * other programs that might try to muck with files as they're
	 * being restored.
	 */
	/* Hopefully, the compiler will optimize this mess into a constant. */
	critical_flags = 0;
#ifdef SF_IMMUTABLE
	critical_flags |= SF_IMMUTABLE;
#endif
#ifdef UF_IMMUTABLE
	critical_flags |= UF_IMMUTABLE;
#endif
#ifdef SF_APPEND
	critical_flags |= SF_APPEND;
#endif
#ifdef UF_APPEND
	critical_flags |= UF_APPEND;
#endif
#ifdef EXT2_APPEND_FL
	critical_flags |= EXT2_APPEND_FL;
#endif
#ifdef EXT2_IMMUTABLE_FL
	critical_flags |= EXT2_IMMUTABLE_FL;
#endif

	if (a->todo & TODO_FFLAGS) {
		archive_entry_fflags(a->entry, &set, &clear);

		/*
		 * The first test encourages the compiler to eliminate
		 * all of this if it's not necessary.
		 */
		if ((critical_flags != 0)  &&  (set & critical_flags)) {
			le = current_fixup(a, a->name);
			if (le == NULL)
				return (ARCHIVE_FATAL);
			le->fixup |= TODO_FFLAGS;
			le->fflags_set = set;
			/* Store the mode if it's not already there. */
			if ((le->fixup & TODO_MODE) == 0)
				le->mode = mode;
		} else {
			r = set_fflags_platform(a, a->fd,
			    a->name, mode, set, clear);
			if (r != ARCHIVE_OK)
				return (r);
		}
	}
	return (ARCHIVE_OK);
}


#if ( defined(HAVE_LCHFLAGS) || defined(HAVE_CHFLAGS) || defined(HAVE_FCHFLAGS) ) && defined(HAVE_STRUCT_STAT_ST_FLAGS)
/*
 * BSD reads flags using stat() and sets them with one of {f,l,}chflags()
 */
static int
set_fflags_platform(struct archive_write_disk *a, int fd, const char *name,
    mode_t mode, unsigned long set, unsigned long clear)
{
	int r;

	(void)mode; /* UNUSED */
	if (set == 0  && clear == 0)
		return (ARCHIVE_OK);

	/*
	 * XXX Is the stat here really necessary?  Or can I just use
	 * the 'set' flags directly?  In particular, I'm not sure
	 * about the correct approach if we're overwriting an existing
	 * file that already has flags on it. XXX
	 */
	if ((r = lazy_stat(a)) != ARCHIVE_OK)
		return (r);

	a->st.st_flags &= ~clear;
	a->st.st_flags |= set;
#ifdef HAVE_FCHFLAGS
	/* If platform has fchflags() and we were given an fd, use it. */
	if (fd >= 0 && fchflags(fd, a->st.st_flags) == 0)
		return (ARCHIVE_OK);
#endif
	/*
	 * If we can't use the fd to set the flags, we'll use the
	 * pathname to set flags.  We prefer lchflags() but will use
	 * chflags() if we must.
	 */
#ifdef HAVE_LCHFLAGS
	if (lchflags(name, a->st.st_flags) == 0)
		return (ARCHIVE_OK);
#elif defined(HAVE_CHFLAGS)
	if (S_ISLNK(a->st.st_mode)) {
		archive_set_error(&a->archive, errno,
		    "Can't set file flags on symlink.");
		return (ARCHIVE_WARN);
	}
	if (chflags(name, a->st.st_flags) == 0)
		return (ARCHIVE_OK);
#endif
	archive_set_error(&a->archive, errno,
	    "Failed to set file flags");
	return (ARCHIVE_WARN);
}

#elif defined(EXT2_IOC_GETFLAGS) && defined(EXT2_IOC_SETFLAGS) && defined(HAVE_WORKING_EXT2_IOC_GETFLAGS)
/*
 * Linux uses ioctl() to read and write file flags.
 */
static int
set_fflags_platform(struct archive_write_disk *a, int fd, const char *name,
    mode_t mode, unsigned long set, unsigned long clear)
{
	int		 ret;
	int		 myfd = fd;
	int newflags, oldflags;
	int sf_mask = 0;

	if (set == 0  && clear == 0)
		return (ARCHIVE_OK);
	/* Only regular files and dirs can have flags. */
	if (!S_ISREG(mode) && !S_ISDIR(mode))
		return (ARCHIVE_OK);

	/* If we weren't given an fd, open it ourselves. */
	if (myfd < 0) {
		myfd = open(name, O_RDONLY | O_NONBLOCK | O_BINARY | O_CLOEXEC);
		__archive_ensure_cloexec_flag(myfd);
	}
	if (myfd < 0)
		return (ARCHIVE_OK);

	/*
	 * Linux has no define for the flags that are only settable by
	 * the root user.  This code may seem a little complex, but
	 * there seem to be some Linux systems that lack these
	 * defines. (?)  The code below degrades reasonably gracefully
	 * if sf_mask is incomplete.
	 */
#ifdef EXT2_IMMUTABLE_FL
	sf_mask |= EXT2_IMMUTABLE_FL;
#endif
#ifdef EXT2_APPEND_FL
	sf_mask |= EXT2_APPEND_FL;
#endif
	/*
	 * XXX As above, this would be way simpler if we didn't have
	 * to read the current flags from disk. XXX
	 */
	ret = ARCHIVE_OK;

	/* Read the current file flags. */
	if (ioctl(myfd, EXT2_IOC_GETFLAGS, &oldflags) < 0)
		goto fail;

	/* Try setting the flags as given. */
	newflags = (oldflags & ~clear) | set;
	if (ioctl(myfd, EXT2_IOC_SETFLAGS, &newflags) >= 0)
		goto cleanup;
	if (errno != EPERM)
		goto fail;

	/* If we couldn't set all the flags, try again with a subset. */
	newflags &= ~sf_mask;
	oldflags &= sf_mask;
	newflags |= oldflags;
	if (ioctl(myfd, EXT2_IOC_SETFLAGS, &newflags) >= 0)
		goto cleanup;

	/* We couldn't set the flags, so report the failure. */
fail:
	archive_set_error(&a->archive, errno,
	    "Failed to set file flags");
	ret = ARCHIVE_WARN;
cleanup:
	if (fd < 0)
		close(myfd);
	return (ret);
}

#else

/*
 * Of course, some systems have neither BSD chflags() nor Linux' flags
 * support through ioctl().
 */
static int
set_fflags_platform(struct archive_write_disk *a, int fd, const char *name,
    mode_t mode, unsigned long set, unsigned long clear)
{
	(void)a; /* UNUSED */
	(void)fd; /* UNUSED */
	(void)name; /* UNUSED */
	(void)mode; /* UNUSED */
	(void)set; /* UNUSED */
	(void)clear; /* UNUSED */
	return (ARCHIVE_OK);
}

#endif /* __linux */

#ifndef HAVE_COPYFILE_H
/* Default is to simply drop Mac extended metadata. */
static int
set_mac_metadata(struct archive_write_disk *a, const char *pathname,
		 const void *metadata, size_t metadata_size)
{
	(void)a; /* UNUSED */
	(void)pathname; /* UNUSED */
	(void)metadata; /* UNUSED */
	(void)metadata_size; /* UNUSED */
	return (ARCHIVE_OK);
}

static int
fixup_appledouble(struct archive_write_disk *a, const char *pathname)
{
	(void)a; /* UNUSED */
	(void)pathname; /* UNUSED */
	return (ARCHIVE_OK);
}
#else

/*
 * On Mac OS, we use copyfile() to unpack the metadata and
 * apply it to the target file.
 */

#if defined(HAVE_SYS_XATTR_H)
static int
copy_xattrs(struct archive_write_disk *a, int tmpfd, int dffd)
{
	ssize_t xattr_size;
	char *xattr_names = NULL, *xattr_val = NULL;
	int ret = ARCHIVE_OK, xattr_i;

	xattr_size = flistxattr(tmpfd, NULL, 0, 0);
	if (xattr_size == -1) {
		archive_set_error(&a->archive, errno,
		    "Failed to read metadata(xattr)");
		ret = ARCHIVE_WARN;
		goto exit_xattr;
	}
	xattr_names = malloc(xattr_size);
	if (xattr_names == NULL) {
		archive_set_error(&a->archive, ENOMEM,
		    "Can't allocate memory for metadata(xattr)");
		ret = ARCHIVE_FATAL;
		goto exit_xattr;
	}
	xattr_size = flistxattr(tmpfd, xattr_names, xattr_size, 0);
	if (xattr_size == -1) {
		archive_set_error(&a->archive, errno,
		    "Failed to read metadata(xattr)");
		ret = ARCHIVE_WARN;
		goto exit_xattr;
	}
	for (xattr_i = 0; xattr_i < xattr_size;
	    xattr_i += strlen(xattr_names + xattr_i) + 1) {
		ssize_t s;
		int f;

		s = fgetxattr(tmpfd, xattr_names + xattr_i, NULL, 0, 0, 0);
		if (s == -1) {
			archive_set_error(&a->archive, errno,
			    "Failed to get metadata(xattr)");
			ret = ARCHIVE_WARN;
			goto exit_xattr;
		}
		xattr_val = realloc(xattr_val, s);
		if (xattr_val == NULL) {
			archive_set_error(&a->archive, ENOMEM,
			    "Failed to get metadata(xattr)");
			ret = ARCHIVE_WARN;
			goto exit_xattr;
		}
		s = fgetxattr(tmpfd, xattr_names + xattr_i, xattr_val, s, 0, 0);
		if (s == -1) {
			archive_set_error(&a->archive, errno,
			    "Failed to get metadata(xattr)");
			ret = ARCHIVE_WARN;
			goto exit_xattr;
		}
		f = fsetxattr(dffd, xattr_names + xattr_i, xattr_val, s, 0, 0);
		if (f == -1) {
			archive_set_error(&a->archive, errno,
			    "Failed to get metadata(xattr)");
			ret = ARCHIVE_WARN;
			goto exit_xattr;
		}
	}
exit_xattr:
	free(xattr_names);
	free(xattr_val);
	return (ret);
}
#endif

static int
copy_acls(struct archive_write_disk *a, int tmpfd, int dffd)
{
	acl_t acl, dfacl = NULL;
	int acl_r, ret = ARCHIVE_OK;

	acl = acl_get_fd(tmpfd);
	if (acl == NULL) {
		if (errno == ENOENT)
			/* There are not any ACLs. */
			return (ret);
		archive_set_error(&a->archive, errno,
		    "Failed to get metadata(acl)");
		ret = ARCHIVE_WARN;
		goto exit_acl;
	}
	dfacl = acl_dup(acl);
	acl_r = acl_set_fd(dffd, dfacl);
	if (acl_r == -1) {
		archive_set_error(&a->archive, errno,
		    "Failed to get metadata(acl)");
		ret = ARCHIVE_WARN;
		goto exit_acl;
	}
exit_acl:
	if (acl)
		acl_free(acl);
	if (dfacl)
		acl_free(dfacl);
	return (ret);
}

static int
create_tempdatafork(struct archive_write_disk *a, const char *pathname)
{
	struct archive_string tmpdatafork;
	int tmpfd;

	archive_string_init(&tmpdatafork);
	archive_strcpy(&tmpdatafork, "tar.md.XXXXXX");
	tmpfd = mkstemp(tmpdatafork.s);
	if (tmpfd < 0) {
		archive_set_error(&a->archive, errno,
		    "Failed to mkstemp");
		archive_string_free(&tmpdatafork);
		return (-1);
	}
	if (copyfile(pathname, tmpdatafork.s, 0,
	    COPYFILE_UNPACK | COPYFILE_NOFOLLOW
	    | COPYFILE_ACL | COPYFILE_XATTR) < 0) {
		archive_set_error(&a->archive, errno,
		    "Failed to restore metadata");
		close(tmpfd);
		tmpfd = -1;
	}
	unlink(tmpdatafork.s);
	archive_string_free(&tmpdatafork);
	return (tmpfd);
}

static int
copy_metadata(struct archive_write_disk *a, const char *metadata,
    const char *datafork, int datafork_compressed)
{
	int ret = ARCHIVE_OK;

	if (datafork_compressed) {
		int dffd, tmpfd;

		tmpfd = create_tempdatafork(a, metadata);
		if (tmpfd == -1)
			return (ARCHIVE_WARN);

		/*
		 * Do not open the data fork compressed by HFS+ compression
		 * with at least a writing mode(O_RDWR or O_WRONLY). it
		 * makes the data fork uncompressed.
		 */
		dffd = open(datafork, 0);
		if (dffd == -1) {
			archive_set_error(&a->archive, errno,
			    "Failed to open the data fork for metadata");
			close(tmpfd);
			return (ARCHIVE_WARN);
		}

#if defined(HAVE_SYS_XATTR_H)
		ret = copy_xattrs(a, tmpfd, dffd);
		if (ret == ARCHIVE_OK)
#endif
			ret = copy_acls(a, tmpfd, dffd);
		close(tmpfd);
		close(dffd);
	} else {
		if (copyfile(metadata, datafork, 0,
		    COPYFILE_UNPACK | COPYFILE_NOFOLLOW
		    | COPYFILE_ACL | COPYFILE_XATTR) < 0) {
			archive_set_error(&a->archive, errno,
			    "Failed to restore metadata");
			ret = ARCHIVE_WARN;
		}
	}
	return (ret);
}

static int
set_mac_metadata(struct archive_write_disk *a, const char *pathname,
		 const void *metadata, size_t metadata_size)
{
	struct archive_string tmp;
	ssize_t written;
	int fd;
	int ret = ARCHIVE_OK;

	/* This would be simpler if copyfile() could just accept the
	 * metadata as a block of memory; then we could sidestep this
	 * silly dance of writing the data to disk just so that
	 * copyfile() can read it back in again. */
	archive_string_init(&tmp);
	archive_strcpy(&tmp, pathname);
	archive_strcat(&tmp, ".XXXXXX");
	fd = mkstemp(tmp.s);

	if (fd < 0) {
		archive_set_error(&a->archive, errno,
				  "Failed to restore metadata");
		archive_string_free(&tmp);
		return (ARCHIVE_WARN);
	}
	written = write(fd, metadata, metadata_size);
	close(fd);
	if ((size_t)written != metadata_size) {
		archive_set_error(&a->archive, errno,
				  "Failed to restore metadata");
		ret = ARCHIVE_WARN;
	} else {
		int compressed;

#if defined(UF_COMPRESSED)
		if ((a->todo & TODO_HFS_COMPRESSION) != 0 &&
		    (ret = lazy_stat(a)) == ARCHIVE_OK)
			compressed = a->st.st_flags & UF_COMPRESSED;
		else
#endif
			compressed = 0;
		ret = copy_metadata(a, tmp.s, pathname, compressed);
	}
	unlink(tmp.s);
	archive_string_free(&tmp);
	return (ret);
}

static int
fixup_appledouble(struct archive_write_disk *a, const char *pathname)
{
	char buff[8];
	struct stat st;
	const char *p;
	struct archive_string datafork;
	int fd = -1, ret = ARCHIVE_OK;

	archive_string_init(&datafork);
	/* Check if the current file name is a type of the resource
	 * fork file. */
	p = strrchr(pathname, '/');
	if (p == NULL)
		p = pathname;
	else
		p++;
	if (p[0] != '.' || p[1] != '_')
		goto skip_appledouble;

	/*
	 * Check if the data fork file exists.
	 *
	 * TODO: Check if this write disk object has handled it.
	 */
	archive_strncpy(&datafork, pathname, p - pathname);
	archive_strcat(&datafork, p + 2);
	if (lstat(datafork.s, &st) == -1 ||
	    (st.st_mode & AE_IFMT) != AE_IFREG)
		goto skip_appledouble;

	/*
	 * Check if the file is in the AppleDouble form.
	 */
	fd = open(pathname, O_RDONLY | O_BINARY | O_CLOEXEC);
	__archive_ensure_cloexec_flag(fd);
	if (fd == -1) {
		archive_set_error(&a->archive, errno,
		    "Failed to open a restoring file");
		ret = ARCHIVE_WARN;
		goto skip_appledouble;
	}
	if (read(fd, buff, 8) == -1) {
		archive_set_error(&a->archive, errno,
		    "Failed to read a restoring file");
		close(fd);
		ret = ARCHIVE_WARN;
		goto skip_appledouble;
	}
	close(fd);
	/* Check AppleDouble Magic Code. */
	if (archive_be32dec(buff) != 0x00051607)
		goto skip_appledouble;
	/* Check AppleDouble Version. */
	if (archive_be32dec(buff+4) != 0x00020000)
		goto skip_appledouble;

	ret = copy_metadata(a, pathname, datafork.s,
#if defined(UF_COMPRESSED)
	    st.st_flags & UF_COMPRESSED);
#else
	    0);
#endif
	if (ret == ARCHIVE_OK) {
		unlink(pathname);
		ret = ARCHIVE_EOF;
	}
skip_appledouble:
	archive_string_free(&datafork);
	return (ret);
}
#endif

#if HAVE_LSETXATTR || HAVE_LSETEA
/*
 * Restore extended attributes -  Linux and AIX implementations:
 * AIX' ea interface is syntaxwise identical to the Linux xattr interface.
 */
static int
set_xattrs(struct archive_write_disk *a)
{
	struct archive_entry *entry = a->entry;
	static int warning_done = 0;
	int ret = ARCHIVE_OK;
	int i = archive_entry_xattr_reset(entry);

	while (i--) {
		const char *name;
		const void *value;
		size_t size;
		archive_entry_xattr_next(entry, &name, &value, &size);
		if (name != NULL &&
				strncmp(name, "xfsroot.", 8) != 0 &&
				strncmp(name, "system.", 7) != 0) {
			int e;
#if HAVE_FSETXATTR
			if (a->fd >= 0)
				e = fsetxattr(a->fd, name, value, size, 0);
			else
#elif HAVE_FSETEA
			if (a->fd >= 0)
				e = fsetea(a->fd, name, value, size, 0);
			else
#endif
			{
#if HAVE_LSETXATTR
				e = lsetxattr(archive_entry_pathname(entry),
				    name, value, size, 0);
#elif HAVE_LSETEA
				e = lsetea(archive_entry_pathname(entry),
				    name, value, size, 0);
#endif
			}
			if (e == -1) {
				if (errno == ENOTSUP || errno == ENOSYS) {
					if (!warning_done) {
						warning_done = 1;
						archive_set_error(&a->archive, errno,
						    "Cannot restore extended "
						    "attributes on this file "
						    "system");
					}
				} else
					archive_set_error(&a->archive, errno,
					    "Failed to set extended attribute");
				ret = ARCHIVE_WARN;
			}
		} else {
			archive_set_error(&a->archive, ARCHIVE_ERRNO_FILE_FORMAT,
			    "Invalid extended attribute encountered");
			ret = ARCHIVE_WARN;
		}
	}
	return (ret);
}
#elif HAVE_EXTATTR_SET_FILE && HAVE_DECL_EXTATTR_NAMESPACE_USER
/*
 * Restore extended attributes -  FreeBSD implementation
 */
static int
set_xattrs(struct archive_write_disk *a)
{
	struct archive_entry *entry = a->entry;
	static int warning_done = 0;
	int ret = ARCHIVE_OK;
	int i = archive_entry_xattr_reset(entry);

	while (i--) {
		const char *name;
		const void *value;
		size_t size;
		archive_entry_xattr_next(entry, &name, &value, &size);
		if (name != NULL) {
			int e;
			int namespace;

			if (strncmp(name, "user.", 5) == 0) {
				/* "user." attributes go to user namespace */
				name += 5;
				namespace = EXTATTR_NAMESPACE_USER;
			} else {
				/* Warn about other extended attributes. */
				archive_set_error(&a->archive,
				    ARCHIVE_ERRNO_FILE_FORMAT,
				    "Can't restore extended attribute ``%s''",
				    name);
				ret = ARCHIVE_WARN;
				continue;
			}
			errno = 0;
#if HAVE_EXTATTR_SET_FD
			if (a->fd >= 0)
				e = extattr_set_fd(a->fd, namespace, name, value, size);
			else
#endif
			/* TODO: should we use extattr_set_link() instead? */
			{
				e = extattr_set_file(archive_entry_pathname(entry),
				    namespace, name, value, size);
			}
			if (e != (int)size) {
				if (errno == ENOTSUP || errno == ENOSYS) {
					if (!warning_done) {
						warning_done = 1;
						archive_set_error(&a->archive, errno,
						    "Cannot restore extended "
						    "attributes on this file "
						    "system");
					}
				} else {
					archive_set_error(&a->archive, errno,
					    "Failed to set extended attribute");
				}

				ret = ARCHIVE_WARN;
			}
		}
	}
	return (ret);
}
#else
/*
 * Restore extended attributes - stub implementation for unsupported systems
 */
static int
set_xattrs(struct archive_write_disk *a)
{
	static int warning_done = 0;

	/* If there aren't any extended attributes, then it's okay not
	 * to extract them, otherwise, issue a single warning. */
	if (archive_entry_xattr_count(a->entry) != 0 && !warning_done) {
		warning_done = 1;
		archive_set_error(&a->archive, ARCHIVE_ERRNO_FILE_FORMAT,
		    "Cannot restore extended attributes on this system");
		return (ARCHIVE_WARN);
	}
	/* Warning was already emitted; suppress further warnings. */
	return (ARCHIVE_OK);
}
#endif

/*
 * Test if file on disk is older than entry.
 */
static int
older(struct stat *st, struct archive_entry *entry)
{
	/* First, test the seconds and return if we have a definite answer. */
	/* Definitely older. */
	if (st->st_mtime < archive_entry_mtime(entry))
		return (1);
	/* Definitely younger. */
	if (st->st_mtime > archive_entry_mtime(entry))
		return (0);
	/* If this platform supports fractional seconds, try those. */
#if HAVE_STRUCT_STAT_ST_MTIMESPEC_TV_NSEC
	/* Definitely older. */
	if (st->st_mtimespec.tv_nsec < archive_entry_mtime_nsec(entry))
		return (1);
#elif HAVE_STRUCT_STAT_ST_MTIM_TV_NSEC
	/* Definitely older. */
	if (st->st_mtim.tv_nsec < archive_entry_mtime_nsec(entry))
		return (1);
#elif HAVE_STRUCT_STAT_ST_MTIME_N
	/* older. */
	if (st->st_mtime_n < archive_entry_mtime_nsec(entry))
		return (1);
#elif HAVE_STRUCT_STAT_ST_UMTIME
	/* older. */
	if (st->st_umtime * 1000 < archive_entry_mtime_nsec(entry))
		return (1);
#elif HAVE_STRUCT_STAT_ST_MTIME_USEC
	/* older. */
	if (st->st_mtime_usec * 1000 < archive_entry_mtime_nsec(entry))
		return (1);
#else
	/* This system doesn't have high-res timestamps. */
#endif
	/* Same age or newer, so not older. */
	return (0);
}

#endif /* !_WIN32 || __CYGWIN__ */

