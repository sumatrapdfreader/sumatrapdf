/*-
 * Copyright (c) 2003-2010 Tim Kientzle
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
__FBSDID("$FreeBSD: head/lib/libarchive/archive_write_disk.c 201159 2009-12-29 05:35:40Z kientzle $");

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_ACL_H
#define _ACL_PRIVATE /* For debugging */
#include <sys/acl.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#include "archive.h"
#include "archive_entry.h"
#include "archive_acl_private.h"
#include "archive_write_disk_private.h"

#if !defined(HAVE_POSIX_ACL) || !defined(ACL_TYPE_NFS4)
/* Default empty function body to satisfy mainline code. */
int
archive_write_disk_set_acls(struct archive *a, int fd, const char *name,
	 struct archive_acl *abstract_acl)
{
	(void)a; /* UNUSED */
	(void)fd; /* UNUSED */
	(void)name; /* UNUSED */
	(void)abstract_acl; /* UNUSED */
	return (ARCHIVE_OK);
}

#else

static int	set_acl(struct archive *, int fd, const char *,
			struct archive_acl *,
			acl_type_t, int archive_entry_acl_type, const char *tn);

/*
 * XXX TODO: What about ACL types other than ACCESS and DEFAULT?
 */
int
archive_write_disk_set_acls(struct archive *a, int fd, const char *name,
	 struct archive_acl *abstract_acl)
{
	int		 ret;

	if (archive_acl_count(abstract_acl, ARCHIVE_ENTRY_ACL_TYPE_POSIX1E) > 0) {
		ret = set_acl(a, fd, name, abstract_acl, ACL_TYPE_ACCESS,
		    ARCHIVE_ENTRY_ACL_TYPE_ACCESS, "access");
		if (ret != ARCHIVE_OK)
			return (ret);
		ret = set_acl(a, fd, name, abstract_acl, ACL_TYPE_DEFAULT,
		    ARCHIVE_ENTRY_ACL_TYPE_DEFAULT, "default");
		return (ret);
	} else if (archive_acl_count(abstract_acl, ARCHIVE_ENTRY_ACL_TYPE_NFS4) > 0) {
		ret = set_acl(a, fd, name, abstract_acl, ACL_TYPE_NFS4,
		    ARCHIVE_ENTRY_ACL_TYPE_NFS4, "nfs4");
		return (ret);
	} else
		return ARCHIVE_OK;
}

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
set_acl(struct archive *a, int fd, const char *name,
    struct archive_acl *abstract_acl,
    acl_type_t acl_type, int ae_requested_type, const char *tname)
{
	acl_t		 acl;
	acl_entry_t	 acl_entry;
	acl_permset_t	 acl_permset;
	acl_flagset_t	 acl_flagset;
	int		 ret;
	int		 ae_type, ae_permset, ae_tag, ae_id;
	uid_t		 ae_uid;
	gid_t		 ae_gid;
	const char	*ae_name;
	int		 entries;
	int		 i;

	ret = ARCHIVE_OK;
	entries = archive_acl_reset(abstract_acl, ae_requested_type);
	if (entries == 0)
		return (ARCHIVE_OK);
	acl = acl_init(entries);
	while (archive_acl_next(a, abstract_acl, ae_requested_type, &ae_type,
		   &ae_permset, &ae_tag, &ae_id, &ae_name) == ARCHIVE_OK) {
		acl_create_entry(&acl, &acl_entry);

		switch (ae_tag) {
		case ARCHIVE_ENTRY_ACL_USER:
			acl_set_tag_type(acl_entry, ACL_USER);
			ae_uid = archive_write_disk_uid(a, ae_name, ae_id);
			acl_set_qualifier(acl_entry, &ae_uid);
			break;
		case ARCHIVE_ENTRY_ACL_GROUP:
			acl_set_tag_type(acl_entry, ACL_GROUP);
			ae_gid = archive_write_disk_gid(a, ae_name, ae_id);
			acl_set_qualifier(acl_entry, &ae_gid);
			break;
		case ARCHIVE_ENTRY_ACL_USER_OBJ:
			acl_set_tag_type(acl_entry, ACL_USER_OBJ);
			break;
		case ARCHIVE_ENTRY_ACL_GROUP_OBJ:
			acl_set_tag_type(acl_entry, ACL_GROUP_OBJ);
			break;
		case ARCHIVE_ENTRY_ACL_MASK:
			acl_set_tag_type(acl_entry, ACL_MASK);
			break;
		case ARCHIVE_ENTRY_ACL_OTHER:
			acl_set_tag_type(acl_entry, ACL_OTHER);
			break;
		case ARCHIVE_ENTRY_ACL_EVERYONE:
			acl_set_tag_type(acl_entry, ACL_EVERYONE);
			break;
		default:
			/* XXX */
			break;
		}

		switch (ae_type) {
		case ARCHIVE_ENTRY_ACL_TYPE_ALLOW:
			acl_set_entry_type_np(acl_entry, ACL_ENTRY_TYPE_ALLOW);
			break;
		case ARCHIVE_ENTRY_ACL_TYPE_DENY:
			acl_set_entry_type_np(acl_entry, ACL_ENTRY_TYPE_DENY);
			break;
		case ARCHIVE_ENTRY_ACL_TYPE_AUDIT:
			acl_set_entry_type_np(acl_entry, ACL_ENTRY_TYPE_AUDIT);
			break;
		case ARCHIVE_ENTRY_ACL_TYPE_ALARM:
			acl_set_entry_type_np(acl_entry, ACL_ENTRY_TYPE_ALARM);
			break;
		case ARCHIVE_ENTRY_ACL_TYPE_ACCESS:
		case ARCHIVE_ENTRY_ACL_TYPE_DEFAULT:
			// These don't translate directly into the system ACL.
			break;
		default:
			// XXX error handling here.
			break;
		}

		acl_get_permset(acl_entry, &acl_permset);
		acl_clear_perms(acl_permset);

		for (i = 0; i < (int)(sizeof(acl_perm_map) / sizeof(acl_perm_map[0])); ++i) {
			if (ae_permset & acl_perm_map[i].archive_perm)
				acl_add_perm(acl_permset,
					     acl_perm_map[i].platform_perm);
		}

		acl_get_flagset_np(acl_entry, &acl_flagset);
		acl_clear_flags_np(acl_flagset);
		for (i = 0; i < (int)(sizeof(acl_inherit_map) / sizeof(acl_inherit_map[0])); ++i) {
			if (ae_permset & acl_inherit_map[i].archive_inherit)
				acl_add_flag_np(acl_flagset,
						acl_inherit_map[i].platform_inherit);
		}
	}

	/* Try restoring the ACL through 'fd' if we can. */
#if HAVE_ACL_SET_FD
	if (fd >= 0 && acl_type == ACL_TYPE_ACCESS && acl_set_fd(fd, acl) == 0)
		ret = ARCHIVE_OK;
	else
#else
#if HAVE_ACL_SET_FD_NP
	if (fd >= 0 && acl_set_fd_np(fd, acl, acl_type) == 0)
		ret = ARCHIVE_OK;
	else
#endif
#endif
#if HAVE_ACL_SET_LINK_NP
	  if (acl_set_link_np(name, acl_type, acl) != 0) {
		archive_set_error(a, errno, "Failed to set %s acl", tname);
		ret = ARCHIVE_WARN;
	  }
#else
	/* TODO: Skip this if 'name' is a symlink. */
	if (acl_set_file(name, acl_type, acl) != 0) {
		archive_set_error(a, errno, "Failed to set %s acl", tname);
		ret = ARCHIVE_WARN;
	}
#endif
	acl_free(acl);
	return (ret);
}
#endif
