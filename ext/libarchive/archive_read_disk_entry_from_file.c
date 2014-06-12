/*-
 * Copyright (c) 2003-2009 Tim Kientzle
 * Copyright (c) 2010-2012 Michihiro NAKAJIMA
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
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
__FBSDID("$FreeBSD: head/lib/libarchive/archive_read_disk_entry_from_file.c 201084 2009-12-28 02:14:09Z kientzle $");

/* This is the tree-walking code for POSIX systems. */
#if !defined(_WIN32) || defined(__CYGWIN__)

#ifdef HAVE_SYS_TYPES_H
/* Mac OSX requires sys/types.h before sys/acl.h. */
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_ACL_H
#include <sys/acl.h>
#endif
#ifdef HAVE_SYS_EXTATTR_H
#include <sys/extattr.h>
#endif
#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif
#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#if defined(HAVE_SYS_XATTR_H)
#include <sys/xattr.h>
#elif defined(HAVE_ATTR_XATTR_H)
#include <attr/xattr.h>
#endif
#ifdef HAVE_SYS_EA_H
#include <sys/ea.h>
#endif
#ifdef HAVE_ACL_LIBACL_H
#include <acl/libacl.h>
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
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif
#ifdef HAVE_LINUX_TYPES_H
#include <linux/types.h>
#endif
#ifdef HAVE_LINUX_FIEMAP_H
#include <linux/fiemap.h>
#endif
#ifdef HAVE_LINUX_FS_H
#include <linux/fs.h>
#endif
/*
 * Some Linux distributions have both linux/ext2_fs.h and ext2fs/ext2_fs.h.
 * As the include guards don't agree, the order of include is important.
 */
#ifdef HAVE_LINUX_EXT2_FS_H
#include <linux/ext2_fs.h>      /* for Linux file flags */
#endif
#if defined(HAVE_EXT2FS_EXT2_FS_H) && !defined(__CYGWIN__)
#include <ext2fs/ext2_fs.h>     /* Linux file flags, broken on Cygwin */
#endif
#ifdef HAVE_PATHS_H
#include <paths.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "archive.h"
#include "archive_entry.h"
#include "archive_private.h"
#include "archive_read_disk_private.h"

#ifndef O_CLOEXEC
#define O_CLOEXEC	0
#endif

/*
 * Linux and FreeBSD plug this obvious hole in POSIX.1e in
 * different ways.
 */
#if HAVE_ACL_GET_PERM
#define	ACL_GET_PERM acl_get_perm
#elif HAVE_ACL_GET_PERM_NP
#define	ACL_GET_PERM acl_get_perm_np
#endif

static int setup_acls(struct archive_read_disk *,
    struct archive_entry *, int *fd);
static int setup_mac_metadata(struct archive_read_disk *,
    struct archive_entry *, int *fd);
static int setup_xattrs(struct archive_read_disk *,
    struct archive_entry *, int *fd);
static int setup_sparse(struct archive_read_disk *,
    struct archive_entry *, int *fd);

int
archive_read_disk_entry_from_file(struct archive *_a,
    struct archive_entry *entry,
    int fd,
    const struct stat *st)
{
	struct archive_read_disk *a = (struct archive_read_disk *)_a;
	const char *path, *name;
	struct stat s;
	int initial_fd = fd;
	int r, r1;

	archive_clear_error(_a);
	path = archive_entry_sourcepath(entry);
	if (path == NULL)
		path = archive_entry_pathname(entry);

	if (a->tree == NULL) {
		if (st == NULL) {
#if HAVE_FSTAT
			if (fd >= 0) {
				if (fstat(fd, &s) != 0) {
					archive_set_error(&a->archive, errno,
					    "Can't fstat");
					return (ARCHIVE_FAILED);
				}
			} else
#endif
#if HAVE_LSTAT
			if (!a->follow_symlinks) {
				if (lstat(path, &s) != 0) {
					archive_set_error(&a->archive, errno,
					    "Can't lstat %s", path);
					return (ARCHIVE_FAILED);
				}
			} else
#endif
			if (stat(path, &s) != 0) {
				archive_set_error(&a->archive, errno,
				    "Can't stat %s", path);
				return (ARCHIVE_FAILED);
			}
			st = &s;
		}
		archive_entry_copy_stat(entry, st);
	}

	/* Lookup uname/gname */
	name = archive_read_disk_uname(_a, archive_entry_uid(entry));
	if (name != NULL)
		archive_entry_copy_uname(entry, name);
	name = archive_read_disk_gname(_a, archive_entry_gid(entry));
	if (name != NULL)
		archive_entry_copy_gname(entry, name);

#ifdef HAVE_STRUCT_STAT_ST_FLAGS
	/* On FreeBSD, we get flags for free with the stat. */
	/* TODO: Does this belong in copy_stat()? */
	if (st->st_flags != 0)
		archive_entry_set_fflags(entry, st->st_flags, 0);
#endif

#if defined(EXT2_IOC_GETFLAGS) && defined(HAVE_WORKING_EXT2_IOC_GETFLAGS)
	/* Linux requires an extra ioctl to pull the flags.  Although
	 * this is an extra step, it has a nice side-effect: We get an
	 * open file descriptor which we can use in the subsequent lookups. */
	if ((S_ISREG(st->st_mode) || S_ISDIR(st->st_mode))) {
		if (fd < 0) {
			if (a->tree != NULL)
				fd = a->open_on_current_dir(a->tree, path,
					O_RDONLY | O_NONBLOCK | O_CLOEXEC);
			else
				fd = open(path, O_RDONLY | O_NONBLOCK |
						O_CLOEXEC);
			__archive_ensure_cloexec_flag(fd);
		}
		if (fd >= 0) {
			int stflags;
			r = ioctl(fd, EXT2_IOC_GETFLAGS, &stflags);
			if (r == 0 && stflags != 0)
				archive_entry_set_fflags(entry, stflags, 0);
		}
	}
#endif

#if defined(HAVE_READLINK) || defined(HAVE_READLINKAT)
	if (S_ISLNK(st->st_mode)) {
		size_t linkbuffer_len = st->st_size + 1;
		char *linkbuffer;
		int lnklen;

		linkbuffer = malloc(linkbuffer_len);
		if (linkbuffer == NULL) {
			archive_set_error(&a->archive, ENOMEM,
			    "Couldn't read link data");
			return (ARCHIVE_FAILED);
		}
		if (a->tree != NULL) {
#ifdef HAVE_READLINKAT
			lnklen = readlinkat(a->tree_current_dir_fd(a->tree),
			    path, linkbuffer, linkbuffer_len);
#else
			if (a->tree_enter_working_dir(a->tree) != 0) {
				archive_set_error(&a->archive, errno,
				    "Couldn't read link data");
				free(linkbuffer);
				return (ARCHIVE_FAILED);
			}
			lnklen = readlink(path, linkbuffer, linkbuffer_len);
#endif /* HAVE_READLINKAT */
		} else
			lnklen = readlink(path, linkbuffer, linkbuffer_len);
		if (lnklen < 0) {
			archive_set_error(&a->archive, errno,
			    "Couldn't read link data");
			free(linkbuffer);
			return (ARCHIVE_FAILED);
		}
		linkbuffer[lnklen] = 0;
		archive_entry_set_symlink(entry, linkbuffer);
		free(linkbuffer);
	}
#endif /* HAVE_READLINK || HAVE_READLINKAT */

	r = setup_acls(a, entry, &fd);
	r1 = setup_xattrs(a, entry, &fd);
	if (r1 < r)
		r = r1;
	if (a->enable_copyfile) {
		r1 = setup_mac_metadata(a, entry, &fd);
		if (r1 < r)
			r = r1;
	}
	r1 = setup_sparse(a, entry, &fd);
	if (r1 < r)
		r = r1;

	/* If we opened the file earlier in this function, close it. */
	if (initial_fd != fd)
		close(fd);
	return (r);
}

#if defined(__APPLE__) && defined(HAVE_COPYFILE_H)
/*
 * The Mac OS "copyfile()" API copies the extended metadata for a
 * file into a separate file in AppleDouble format (see RFC 1740).
 *
 * Mac OS tar and cpio implementations store this extended
 * metadata as a separate entry just before the regular entry
 * with a "._" prefix added to the filename.
 *
 * Note that this is currently done unconditionally; the tar program has
 * an option to discard this information before the archive is written.
 *
 * TODO: If there's a failure, report it and return ARCHIVE_WARN.
 */
static int
setup_mac_metadata(struct archive_read_disk *a,
    struct archive_entry *entry, int *fd)
{
	int tempfd = -1;
	int copyfile_flags = COPYFILE_NOFOLLOW | COPYFILE_ACL | COPYFILE_XATTR;
	struct stat copyfile_stat;
	int ret = ARCHIVE_OK;
	void *buff = NULL;
	int have_attrs;
	const char *name, *tempdir;
	struct archive_string tempfile;

	(void)fd; /* UNUSED */
	name = archive_entry_sourcepath(entry);
	if (name == NULL)
		name = archive_entry_pathname(entry);
	if (name == NULL) {
		archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
		    "Can't open file to read extended attributes: No name");
		return (ARCHIVE_WARN);
	}

	if (a->tree != NULL) {
		if (a->tree_enter_working_dir(a->tree) != 0) {
			archive_set_error(&a->archive, errno,
				    "Couldn't change dir");
				return (ARCHIVE_FAILED);
		}
	}

	/* Short-circuit if there's nothing to do. */
	have_attrs = copyfile(name, NULL, 0, copyfile_flags | COPYFILE_CHECK);
	if (have_attrs == -1) {
		archive_set_error(&a->archive, errno,
			"Could not check extended attributes");
		return (ARCHIVE_WARN);
	}
	if (have_attrs == 0)
		return (ARCHIVE_OK);

	tempdir = NULL;
	if (issetugid() == 0)
		tempdir = getenv("TMPDIR");
	if (tempdir == NULL)
		tempdir = _PATH_TMP;
	archive_string_init(&tempfile);
	archive_strcpy(&tempfile, tempdir);
	archive_strcat(&tempfile, "tar.md.XXXXXX");
	tempfd = mkstemp(tempfile.s);
	if (tempfd < 0) {
		archive_set_error(&a->archive, errno,
		    "Could not open extended attribute file");
		ret = ARCHIVE_WARN;
		goto cleanup;
	}
	__archive_ensure_cloexec_flag(tempfd);

	/* XXX I wish copyfile() could pack directly to a memory
	 * buffer; that would avoid the temp file here.  For that
	 * matter, it would be nice if fcopyfile() actually worked,
	 * that would reduce the many open/close races here. */
	if (copyfile(name, tempfile.s, 0, copyfile_flags | COPYFILE_PACK)) {
		archive_set_error(&a->archive, errno,
		    "Could not pack extended attributes");
		ret = ARCHIVE_WARN;
		goto cleanup;
	}
	if (fstat(tempfd, &copyfile_stat)) {
		archive_set_error(&a->archive, errno,
		    "Could not check size of extended attributes");
		ret = ARCHIVE_WARN;
		goto cleanup;
	}
	buff = malloc(copyfile_stat.st_size);
	if (buff == NULL) {
		archive_set_error(&a->archive, errno,
		    "Could not allocate memory for extended attributes");
		ret = ARCHIVE_WARN;
		goto cleanup;
	}
	if (copyfile_stat.st_size != read(tempfd, buff, copyfile_stat.st_size)) {
		archive_set_error(&a->archive, errno,
		    "Could not read extended attributes into memory");
		ret = ARCHIVE_WARN;
		goto cleanup;
	}
	archive_entry_copy_mac_metadata(entry, buff, copyfile_stat.st_size);

cleanup:
	if (tempfd >= 0) {
		close(tempfd);
		unlink(tempfile.s);
	}
	archive_string_free(&tempfile);
	free(buff);
	return (ret);
}

#else

/*
 * Stub implementation for non-Mac systems.
 */
static int
setup_mac_metadata(struct archive_read_disk *a,
    struct archive_entry *entry, int *fd)
{
	(void)a; /* UNUSED */
	(void)entry; /* UNUSED */
	(void)fd; /* UNUSED */
	return (ARCHIVE_OK);
}
#endif


#if defined(HAVE_POSIX_ACL) && defined(ACL_TYPE_NFS4)
static int translate_acl(struct archive_read_disk *a,
    struct archive_entry *entry, acl_t acl, int archive_entry_acl_type);

static int
setup_acls(struct archive_read_disk *a,
    struct archive_entry *entry, int *fd)
{
	const char	*accpath;
	acl_t		 acl;
#if HAVE_ACL_IS_TRIVIAL_NP
	int		r;
#endif

	accpath = archive_entry_sourcepath(entry);
	if (accpath == NULL)
		accpath = archive_entry_pathname(entry);

	archive_entry_acl_clear(entry);

	/* Try NFS4 ACL first. */
	if (*fd >= 0)
		acl = acl_get_fd(*fd);
#if HAVE_ACL_GET_LINK_NP
	else if (!a->follow_symlinks)
		acl = acl_get_link_np(accpath, ACL_TYPE_NFS4);
#else
	else if ((!a->follow_symlinks)
	    && (archive_entry_filetype(entry) == AE_IFLNK))
		/* We can't get the ACL of a symlink, so we assume it can't
		   have one. */
		acl = NULL;
#endif
	else
		acl = acl_get_file(accpath, ACL_TYPE_NFS4);
#if HAVE_ACL_IS_TRIVIAL_NP
	/* Ignore "trivial" ACLs that just mirror the file mode. */
	acl_is_trivial_np(acl, &r);
	if (r) {
		acl_free(acl);
		acl = NULL;
	}
#endif
	if (acl != NULL) {
		translate_acl(a, entry, acl, ARCHIVE_ENTRY_ACL_TYPE_NFS4);
		acl_free(acl);
		return (ARCHIVE_OK);
	}

	/* Retrieve access ACL from file. */
	if (*fd >= 0)
		acl = acl_get_fd(*fd);
#if HAVE_ACL_GET_LINK_NP
	else if (!a->follow_symlinks)
		acl = acl_get_link_np(accpath, ACL_TYPE_ACCESS);
#else
	else if ((!a->follow_symlinks)
	    && (archive_entry_filetype(entry) == AE_IFLNK))
		/* We can't get the ACL of a symlink, so we assume it can't
		   have one. */
		acl = NULL;
#endif
	else
		acl = acl_get_file(accpath, ACL_TYPE_ACCESS);
	if (acl != NULL) {
		translate_acl(a, entry, acl,
		    ARCHIVE_ENTRY_ACL_TYPE_ACCESS);
		acl_free(acl);
	}

	/* Only directories can have default ACLs. */
	if (S_ISDIR(archive_entry_mode(entry))) {
		acl = acl_get_file(accpath, ACL_TYPE_DEFAULT);
		if (acl != NULL) {
			translate_acl(a, entry, acl,
			    ARCHIVE_ENTRY_ACL_TYPE_DEFAULT);
			acl_free(acl);
		}
	}
	return (ARCHIVE_OK);
}

/*
 * Translate system ACL into libarchive internal structure.
 */

static struct {
        int archive_perm;
        int platform_perm;
} acl_perm_map[] = {
        {ARCHIVE_ENTRY_ACL_EXECUTE, ACL_EXECUTE},
        {ARCHIVE_ENTRY_ACL_WRITE, ACL_WRITE},
        {ARCHIVE_ENTRY_ACL_READ, ACL_READ},
        {ARCHIVE_ENTRY_ACL_READ_DATA, ACL_READ_DATA},
        {ARCHIVE_ENTRY_ACL_LIST_DIRECTORY, ACL_LIST_DIRECTORY},
        {ARCHIVE_ENTRY_ACL_WRITE_DATA, ACL_WRITE_DATA},
        {ARCHIVE_ENTRY_ACL_ADD_FILE, ACL_ADD_FILE},
        {ARCHIVE_ENTRY_ACL_APPEND_DATA, ACL_APPEND_DATA},
        {ARCHIVE_ENTRY_ACL_ADD_SUBDIRECTORY, ACL_ADD_SUBDIRECTORY},
        {ARCHIVE_ENTRY_ACL_READ_NAMED_ATTRS, ACL_READ_NAMED_ATTRS},
        {ARCHIVE_ENTRY_ACL_WRITE_NAMED_ATTRS, ACL_WRITE_NAMED_ATTRS},
        {ARCHIVE_ENTRY_ACL_DELETE_CHILD, ACL_DELETE_CHILD},
        {ARCHIVE_ENTRY_ACL_READ_ATTRIBUTES, ACL_READ_ATTRIBUTES},
        {ARCHIVE_ENTRY_ACL_WRITE_ATTRIBUTES, ACL_WRITE_ATTRIBUTES},
        {ARCHIVE_ENTRY_ACL_DELETE, ACL_DELETE},
        {ARCHIVE_ENTRY_ACL_READ_ACL, ACL_READ_ACL},
        {ARCHIVE_ENTRY_ACL_WRITE_ACL, ACL_WRITE_ACL},
        {ARCHIVE_ENTRY_ACL_WRITE_OWNER, ACL_WRITE_OWNER},
        {ARCHIVE_ENTRY_ACL_SYNCHRONIZE, ACL_SYNCHRONIZE}
};

static struct {
        int archive_inherit;
        int platform_inherit;
} acl_inherit_map[] = {
        {ARCHIVE_ENTRY_ACL_ENTRY_FILE_INHERIT, ACL_ENTRY_FILE_INHERIT},
	{ARCHIVE_ENTRY_ACL_ENTRY_DIRECTORY_INHERIT, ACL_ENTRY_DIRECTORY_INHERIT},
	{ARCHIVE_ENTRY_ACL_ENTRY_NO_PROPAGATE_INHERIT, ACL_ENTRY_NO_PROPAGATE_INHERIT},
	{ARCHIVE_ENTRY_ACL_ENTRY_INHERIT_ONLY, ACL_ENTRY_INHERIT_ONLY}
};

static int
translate_acl(struct archive_read_disk *a,
    struct archive_entry *entry, acl_t acl, int default_entry_acl_type)
{
	acl_tag_t	 acl_tag;
	acl_entry_type_t acl_type;
	acl_flagset_t	 acl_flagset;
	acl_entry_t	 acl_entry;
	acl_permset_t	 acl_permset;
	int		 brand, i, r, entry_acl_type;
	int		 s, ae_id, ae_tag, ae_perm;
	const char	*ae_name;


	// FreeBSD "brands" ACLs as POSIX.1e or NFSv4
	// Make sure the "brand" on this ACL is consistent
	// with the default_entry_acl_type bits provided.
	acl_get_brand_np(acl, &brand);
	switch (brand) {
	case ACL_BRAND_POSIX:
		switch (default_entry_acl_type) {
		case ARCHIVE_ENTRY_ACL_TYPE_ACCESS:
		case ARCHIVE_ENTRY_ACL_TYPE_DEFAULT:
			break;
		default:
			// XXX set warning message?
			return ARCHIVE_FAILED;
		}
		break;
	case ACL_BRAND_NFS4:
		if (default_entry_acl_type & ~ARCHIVE_ENTRY_ACL_TYPE_NFS4) {
			// XXX set warning message?
			return ARCHIVE_FAILED;
		}
		break;
	default:
		// XXX set warning message?
		return ARCHIVE_FAILED;
		break;
	}


	s = acl_get_entry(acl, ACL_FIRST_ENTRY, &acl_entry);
	while (s == 1) {
		ae_id = -1;
		ae_name = NULL;
		ae_perm = 0;

		acl_get_tag_type(acl_entry, &acl_tag);
		switch (acl_tag) {
		case ACL_USER:
			ae_id = (int)*(uid_t *)acl_get_qualifier(acl_entry);
			ae_name = archive_read_disk_uname(&a->archive, ae_id);
			ae_tag = ARCHIVE_ENTRY_ACL_USER;
			break;
		case ACL_GROUP:
			ae_id = (int)*(gid_t *)acl_get_qualifier(acl_entry);
			ae_name = archive_read_disk_gname(&a->archive, ae_id);
			ae_tag = ARCHIVE_ENTRY_ACL_GROUP;
			break;
		case ACL_MASK:
			ae_tag = ARCHIVE_ENTRY_ACL_MASK;
			break;
		case ACL_USER_OBJ:
			ae_tag = ARCHIVE_ENTRY_ACL_USER_OBJ;
			break;
		case ACL_GROUP_OBJ:
			ae_tag = ARCHIVE_ENTRY_ACL_GROUP_OBJ;
			break;
		case ACL_OTHER:
			ae_tag = ARCHIVE_ENTRY_ACL_OTHER;
			break;
		case ACL_EVERYONE:
			ae_tag = ARCHIVE_ENTRY_ACL_EVERYONE;
			break;
		default:
			/* Skip types that libarchive can't support. */
			s = acl_get_entry(acl, ACL_NEXT_ENTRY, &acl_entry);
			continue;
		}

		// XXX acl type maps to allow/deny/audit/YYYY bits
		// XXX acl_get_entry_type_np on FreeBSD returns EINVAL for
		// non-NFSv4 ACLs
		entry_acl_type = default_entry_acl_type;
		r = acl_get_entry_type_np(acl_entry, &acl_type);
		if (r == 0) {
			switch (acl_type) {
			case ACL_ENTRY_TYPE_ALLOW:
				entry_acl_type = ARCHIVE_ENTRY_ACL_TYPE_ALLOW;
				break;
			case ACL_ENTRY_TYPE_DENY:
				entry_acl_type = ARCHIVE_ENTRY_ACL_TYPE_DENY;
				break;
			case ACL_ENTRY_TYPE_AUDIT:
				entry_acl_type = ARCHIVE_ENTRY_ACL_TYPE_AUDIT;
				break;
			case ACL_ENTRY_TYPE_ALARM:
				entry_acl_type = ARCHIVE_ENTRY_ACL_TYPE_ALARM;
				break;
			}
		}

		/*
		 * Libarchive stores "flag" (NFSv4 inheritance bits)
		 * in the ae_perm bitmap.
		 */
		acl_get_flagset_np(acl_entry, &acl_flagset);
                for (i = 0; i < (int)(sizeof(acl_inherit_map) / sizeof(acl_inherit_map[0])); ++i) {
			if (acl_get_flag_np(acl_flagset,
					    acl_inherit_map[i].platform_inherit))
				ae_perm |= acl_inherit_map[i].archive_inherit;

                }

		acl_get_permset(acl_entry, &acl_permset);
                for (i = 0; i < (int)(sizeof(acl_perm_map) / sizeof(acl_perm_map[0])); ++i) {
			/*
			 * acl_get_perm() is spelled differently on different
			 * platforms; see above.
			 */
			if (ACL_GET_PERM(acl_permset, acl_perm_map[i].platform_perm))
				ae_perm |= acl_perm_map[i].archive_perm;
		}

		archive_entry_acl_add_entry(entry, entry_acl_type,
					    ae_perm, ae_tag,
					    ae_id, ae_name);

		s = acl_get_entry(acl, ACL_NEXT_ENTRY, &acl_entry);
	}
	return (ARCHIVE_OK);
}
#else
static int
setup_acls(struct archive_read_disk *a,
    struct archive_entry *entry, int *fd)
{
	(void)a;      /* UNUSED */
	(void)entry;  /* UNUSED */
	(void)fd;     /* UNUSED */
	return (ARCHIVE_OK);
}
#endif

#if (HAVE_FGETXATTR && HAVE_FLISTXATTR && HAVE_LISTXATTR && \
    HAVE_LLISTXATTR && HAVE_GETXATTR && HAVE_LGETXATTR) || \
    (HAVE_FGETEA && HAVE_FLISTEA && HAVE_LISTEA)

/*
 * Linux and AIX extended attribute support.
 *
 * TODO:  By using a stack-allocated buffer for the first
 * call to getxattr(), we might be able to avoid the second
 * call entirely.  We only need the second call if the
 * stack-allocated buffer is too small.  But a modest buffer
 * of 1024 bytes or so will often be big enough.  Same applies
 * to listxattr().
 */


static int
setup_xattr(struct archive_read_disk *a,
    struct archive_entry *entry, const char *name, int fd)
{
	ssize_t size;
	void *value = NULL;
	const char *accpath;

	accpath = archive_entry_sourcepath(entry);
	if (accpath == NULL)
		accpath = archive_entry_pathname(entry);

#if HAVE_FGETXATTR
	if (fd >= 0)
		size = fgetxattr(fd, name, NULL, 0);
	else if (!a->follow_symlinks)
		size = lgetxattr(accpath, name, NULL, 0);
	else
		size = getxattr(accpath, name, NULL, 0);
#elif HAVE_FGETEA
	if (fd >= 0)
		size = fgetea(fd, name, NULL, 0);
	else if (!a->follow_symlinks)
		size = lgetea(accpath, name, NULL, 0);
	else
		size = getea(accpath, name, NULL, 0);
#endif

	if (size == -1) {
		archive_set_error(&a->archive, errno,
		    "Couldn't query extended attribute");
		return (ARCHIVE_WARN);
	}

	if (size > 0 && (value = malloc(size)) == NULL) {
		archive_set_error(&a->archive, errno, "Out of memory");
		return (ARCHIVE_FATAL);
	}

#if HAVE_FGETXATTR
	if (fd >= 0)
		size = fgetxattr(fd, name, value, size);
	else if (!a->follow_symlinks)
		size = lgetxattr(accpath, name, value, size);
	else
		size = getxattr(accpath, name, value, size);
#elif HAVE_FGETEA
	if (fd >= 0)
		size = fgetea(fd, name, value, size);
	else if (!a->follow_symlinks)
		size = lgetea(accpath, name, value, size);
	else
		size = getea(accpath, name, value, size);
#endif

	if (size == -1) {
		archive_set_error(&a->archive, errno,
		    "Couldn't read extended attribute");
		return (ARCHIVE_WARN);
	}

	archive_entry_xattr_add_entry(entry, name, value, size);

	free(value);
	return (ARCHIVE_OK);
}

static int
setup_xattrs(struct archive_read_disk *a,
    struct archive_entry *entry, int *fd)
{
	char *list, *p;
	const char *path;
	ssize_t list_size;

	path = archive_entry_sourcepath(entry);
	if (path == NULL)
		path = archive_entry_pathname(entry);

	if (*fd < 0 && a->tree != NULL) {
		if (a->follow_symlinks ||
		    archive_entry_filetype(entry) != AE_IFLNK)
			*fd = a->open_on_current_dir(a->tree, path,
				O_RDONLY | O_NONBLOCK);
		if (*fd < 0) {
			if (a->tree_enter_working_dir(a->tree) != 0) {
				archive_set_error(&a->archive, errno,
				    "Couldn't access %s", path);
				return (ARCHIVE_FAILED);
			}
		}
	}

#if HAVE_FLISTXATTR
	if (*fd >= 0)
		list_size = flistxattr(*fd, NULL, 0);
	else if (!a->follow_symlinks)
		list_size = llistxattr(path, NULL, 0);
	else
		list_size = listxattr(path, NULL, 0);
#elif HAVE_FLISTEA
	if (*fd >= 0)
		list_size = flistea(*fd, NULL, 0);
	else if (!a->follow_symlinks)
		list_size = llistea(path, NULL, 0);
	else
		list_size = listea(path, NULL, 0);
#endif

	if (list_size == -1) {
		if (errno == ENOTSUP || errno == ENOSYS)
			return (ARCHIVE_OK);
		archive_set_error(&a->archive, errno,
			"Couldn't list extended attributes");
		return (ARCHIVE_WARN);
	}

	if (list_size == 0)
		return (ARCHIVE_OK);

	if ((list = malloc(list_size)) == NULL) {
		archive_set_error(&a->archive, errno, "Out of memory");
		return (ARCHIVE_FATAL);
	}

#if HAVE_FLISTXATTR
	if (*fd >= 0)
		list_size = flistxattr(*fd, list, list_size);
	else if (!a->follow_symlinks)
		list_size = llistxattr(path, list, list_size);
	else
		list_size = listxattr(path, list, list_size);
#elif HAVE_FLISTEA
	if (*fd >= 0)
		list_size = flistea(*fd, list, list_size);
	else if (!a->follow_symlinks)
		list_size = llistea(path, list, list_size);
	else
		list_size = listea(path, list, list_size);
#endif

	if (list_size == -1) {
		archive_set_error(&a->archive, errno,
			"Couldn't retrieve extended attributes");
		free(list);
		return (ARCHIVE_WARN);
	}

	for (p = list; (p - list) < list_size; p += strlen(p) + 1) {
		if (strncmp(p, "system.", 7) == 0 ||
				strncmp(p, "xfsroot.", 8) == 0)
			continue;
		setup_xattr(a, entry, p, *fd);
	}

	free(list);
	return (ARCHIVE_OK);
}

#elif HAVE_EXTATTR_GET_FILE && HAVE_EXTATTR_LIST_FILE && \
    HAVE_DECL_EXTATTR_NAMESPACE_USER

/*
 * FreeBSD extattr interface.
 */

/* TODO: Implement this.  Follow the Linux model above, but
 * with FreeBSD-specific system calls, of course.  Be careful
 * to not include the system extattrs that hold ACLs; we handle
 * those separately.
 */
static int
setup_xattr(struct archive_read_disk *a, struct archive_entry *entry,
    int namespace, const char *name, const char *fullname, int fd);

static int
setup_xattr(struct archive_read_disk *a, struct archive_entry *entry,
    int namespace, const char *name, const char *fullname, int fd)
{
	ssize_t size;
	void *value = NULL;
	const char *accpath;

	accpath = archive_entry_sourcepath(entry);
	if (accpath == NULL)
		accpath = archive_entry_pathname(entry);

	if (fd >= 0)
		size = extattr_get_fd(fd, namespace, name, NULL, 0);
	else if (!a->follow_symlinks)
		size = extattr_get_link(accpath, namespace, name, NULL, 0);
	else
		size = extattr_get_file(accpath, namespace, name, NULL, 0);

	if (size == -1) {
		archive_set_error(&a->archive, errno,
		    "Couldn't query extended attribute");
		return (ARCHIVE_WARN);
	}

	if (size > 0 && (value = malloc(size)) == NULL) {
		archive_set_error(&a->archive, errno, "Out of memory");
		return (ARCHIVE_FATAL);
	}

	if (fd >= 0)
		size = extattr_get_fd(fd, namespace, name, value, size);
	else if (!a->follow_symlinks)
		size = extattr_get_link(accpath, namespace, name, value, size);
	else
		size = extattr_get_file(accpath, namespace, name, value, size);

	if (size == -1) {
		free(value);
		archive_set_error(&a->archive, errno,
		    "Couldn't read extended attribute");
		return (ARCHIVE_WARN);
	}

	archive_entry_xattr_add_entry(entry, fullname, value, size);

	free(value);
	return (ARCHIVE_OK);
}

static int
setup_xattrs(struct archive_read_disk *a,
    struct archive_entry *entry, int *fd)
{
	char buff[512];
	char *list, *p;
	ssize_t list_size;
	const char *path;
	int namespace = EXTATTR_NAMESPACE_USER;

	path = archive_entry_sourcepath(entry);
	if (path == NULL)
		path = archive_entry_pathname(entry);

	if (*fd < 0 && a->tree != NULL) {
		if (a->follow_symlinks ||
		    archive_entry_filetype(entry) != AE_IFLNK)
			*fd = a->open_on_current_dir(a->tree, path,
				O_RDONLY | O_NONBLOCK);
		if (*fd < 0) {
			if (a->tree_enter_working_dir(a->tree) != 0) {
				archive_set_error(&a->archive, errno,
				    "Couldn't access %s", path);
				return (ARCHIVE_FAILED);
			}
		}
	}

	if (*fd >= 0)
		list_size = extattr_list_fd(*fd, namespace, NULL, 0);
	else if (!a->follow_symlinks)
		list_size = extattr_list_link(path, namespace, NULL, 0);
	else
		list_size = extattr_list_file(path, namespace, NULL, 0);

	if (list_size == -1 && errno == EOPNOTSUPP)
		return (ARCHIVE_OK);
	if (list_size == -1) {
		archive_set_error(&a->archive, errno,
			"Couldn't list extended attributes");
		return (ARCHIVE_WARN);
	}

	if (list_size == 0)
		return (ARCHIVE_OK);

	if ((list = malloc(list_size)) == NULL) {
		archive_set_error(&a->archive, errno, "Out of memory");
		return (ARCHIVE_FATAL);
	}

	if (*fd >= 0)
		list_size = extattr_list_fd(*fd, namespace, list, list_size);
	else if (!a->follow_symlinks)
		list_size = extattr_list_link(path, namespace, list, list_size);
	else
		list_size = extattr_list_file(path, namespace, list, list_size);

	if (list_size == -1) {
		archive_set_error(&a->archive, errno,
			"Couldn't retrieve extended attributes");
		free(list);
		return (ARCHIVE_WARN);
	}

	p = list;
	while ((p - list) < list_size) {
		size_t len = 255 & (int)*p;
		char *name;

		strcpy(buff, "user.");
		name = buff + strlen(buff);
		memcpy(name, p + 1, len);
		name[len] = '\0';
		setup_xattr(a, entry, namespace, name, buff, *fd);
		p += 1 + len;
	}

	free(list);
	return (ARCHIVE_OK);
}

#else

/*
 * Generic (stub) extended attribute support.
 */
static int
setup_xattrs(struct archive_read_disk *a,
    struct archive_entry *entry, int *fd)
{
	(void)a;     /* UNUSED */
	(void)entry; /* UNUSED */
	(void)fd;    /* UNUSED */
	return (ARCHIVE_OK);
}

#endif

#if defined(HAVE_LINUX_FIEMAP_H)

/*
 * Linux sparse interface.
 *
 * The FIEMAP ioctl returns an "extent" for each physical allocation
 * on disk.  We need to process those to generate a more compact list
 * of logical file blocks.  We also need to be very careful to use
 * FIEMAP_FLAG_SYNC here, since there are reports that Linux sometimes
 * does not report allocations for newly-written data that hasn't
 * been synced to disk.
 *
 * It's important to return a minimal sparse file list because we want
 * to not trigger sparse file extensions if we don't have to, since
 * not all readers support them.
 */

static int
setup_sparse(struct archive_read_disk *a,
    struct archive_entry *entry, int *fd)
{
	char buff[4096];
	struct fiemap *fm;
	struct fiemap_extent *fe;
	int64_t size;
	int count, do_fiemap;
	int exit_sts = ARCHIVE_OK;

	if (archive_entry_filetype(entry) != AE_IFREG
	    || archive_entry_size(entry) <= 0
	    || archive_entry_hardlink(entry) != NULL)
		return (ARCHIVE_OK);

	if (*fd < 0) {
		const char *path;

		path = archive_entry_sourcepath(entry);
		if (path == NULL)
			path = archive_entry_pathname(entry);
		if (a->tree != NULL)
			*fd = a->open_on_current_dir(a->tree, path,
				O_RDONLY | O_NONBLOCK | O_CLOEXEC);
		else
			*fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
		if (*fd < 0) {
			archive_set_error(&a->archive, errno,
			    "Can't open `%s'", path);
			return (ARCHIVE_FAILED);
		}
		__archive_ensure_cloexec_flag(*fd);
	}

	/* Initialize buffer to avoid the error valgrind complains about. */
	memset(buff, 0, sizeof(buff));
	count = (sizeof(buff) - sizeof(*fm))/sizeof(*fe);
	fm = (struct fiemap *)buff;
	fm->fm_start = 0;
	fm->fm_length = ~0ULL;;
	fm->fm_flags = FIEMAP_FLAG_SYNC;
	fm->fm_extent_count = count;
	do_fiemap = 1;
	size = archive_entry_size(entry);
	for (;;) {
		int i, r;

		r = ioctl(*fd, FS_IOC_FIEMAP, fm); 
		if (r < 0) {
			/* When something error happens, it is better we
			 * should return ARCHIVE_OK because an earlier
			 * version(<2.6.28) cannot perfom FS_IOC_FIEMAP. */
			goto exit_setup_sparse;
		}
		if (fm->fm_mapped_extents == 0)
			break;
		fe = fm->fm_extents;
		for (i = 0; i < (int)fm->fm_mapped_extents; i++, fe++) {
			if (!(fe->fe_flags & FIEMAP_EXTENT_UNWRITTEN)) {
				/* The fe_length of the last block does not
				 * adjust itself to its size files. */
				int64_t length = fe->fe_length;
				if (fe->fe_logical + length > (uint64_t)size)
					length -= fe->fe_logical + length - size;
				if (fe->fe_logical == 0 && length == size) {
					/* This is not sparse. */
					do_fiemap = 0;
					break;
				}
				if (length > 0)
					archive_entry_sparse_add_entry(entry,
					    fe->fe_logical, length);
			}
			if (fe->fe_flags & FIEMAP_EXTENT_LAST)
				do_fiemap = 0;
		}
		if (do_fiemap) {
			fe = fm->fm_extents + fm->fm_mapped_extents -1;
			fm->fm_start = fe->fe_logical + fe->fe_length;
		} else
			break;
	}
exit_setup_sparse:
	return (exit_sts);
}

#elif defined(SEEK_HOLE) && defined(SEEK_DATA) && defined(_PC_MIN_HOLE_SIZE)

/*
 * FreeBSD and Solaris sparse interface.
 */

static int
setup_sparse(struct archive_read_disk *a,
    struct archive_entry *entry, int *fd)
{
	int64_t size;
	off_t initial_off; /* FreeBSD/Solaris only, so off_t okay here */
	off_t off_s, off_e; /* FreeBSD/Solaris only, so off_t okay here */
	int exit_sts = ARCHIVE_OK;

	if (archive_entry_filetype(entry) != AE_IFREG
	    || archive_entry_size(entry) <= 0
	    || archive_entry_hardlink(entry) != NULL)
		return (ARCHIVE_OK);

	/* Does filesystem support the reporting of hole ? */
	if (*fd < 0 && a->tree != NULL) {
		const char *path;

		path = archive_entry_sourcepath(entry);
		if (path == NULL)
			path = archive_entry_pathname(entry);
		*fd = a->open_on_current_dir(a->tree, path,
				O_RDONLY | O_NONBLOCK);
		if (*fd < 0) {
			archive_set_error(&a->archive, errno,
			    "Can't open `%s'", path);
			return (ARCHIVE_FAILED);
		}
	}

	if (*fd >= 0) {
		if (fpathconf(*fd, _PC_MIN_HOLE_SIZE) <= 0)
			return (ARCHIVE_OK);
		initial_off = lseek(*fd, 0, SEEK_CUR);
		if (initial_off != 0)
			lseek(*fd, 0, SEEK_SET);
	} else {
		const char *path;

		path = archive_entry_sourcepath(entry);
		if (path == NULL)
			path = archive_entry_pathname(entry);
			
		if (pathconf(path, _PC_MIN_HOLE_SIZE) <= 0)
			return (ARCHIVE_OK);
		*fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
		if (*fd < 0) {
			archive_set_error(&a->archive, errno,
			    "Can't open `%s'", path);
			return (ARCHIVE_FAILED);
		}
		__archive_ensure_cloexec_flag(*fd);
		initial_off = 0;
	}

	off_s = 0;
	size = archive_entry_size(entry);
	while (off_s < size) {
		off_s = lseek(*fd, off_s, SEEK_DATA);
		if (off_s == (off_t)-1) {
			if (errno == ENXIO)
				break;/* no more hole */
			archive_set_error(&a->archive, errno,
			    "lseek(SEEK_HOLE) failed");
			exit_sts = ARCHIVE_FAILED;
			goto exit_setup_sparse;
		}
		off_e = lseek(*fd, off_s, SEEK_HOLE);
		if (off_e == (off_t)-1) {
			if (errno == ENXIO) {
				off_e = lseek(*fd, 0, SEEK_END);
				if (off_e != (off_t)-1)
					break;/* no more data */
			}
			archive_set_error(&a->archive, errno,
			    "lseek(SEEK_DATA) failed");
			exit_sts = ARCHIVE_FAILED;
			goto exit_setup_sparse;
		}
		if (off_s == 0 && off_e == size)
			break;/* This is not spase. */
		archive_entry_sparse_add_entry(entry, off_s,
			off_e - off_s);
		off_s = off_e;
	}
exit_setup_sparse:
	lseek(*fd, initial_off, SEEK_SET);
	return (exit_sts);
}

#else

/*
 * Generic (stub) sparse support.
 */
static int
setup_sparse(struct archive_read_disk *a,
    struct archive_entry *entry, int *fd)
{
	(void)a;     /* UNUSED */
	(void)entry; /* UNUSED */
	(void)fd;    /* UNUSED */
	return (ARCHIVE_OK);
}

#endif

#endif /* !defined(_WIN32) || defined(__CYGWIN__) */

