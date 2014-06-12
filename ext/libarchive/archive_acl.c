/*-
 * Copyright (c) 2003-2010 Tim Kientzle
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
__FBSDID("$FreeBSD$");

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif
#ifdef HAVE_WCHAR_H
#include <wchar.h>
#endif

#include "archive_acl_private.h"
#include "archive_entry.h"
#include "archive_private.h"

#undef max
#define	max(a, b)	((a)>(b)?(a):(b))

#ifndef HAVE_WMEMCMP
/* Good enough for simple equality testing, but not for sorting. */
#define wmemcmp(a,b,i)  memcmp((a), (b), (i) * sizeof(wchar_t))
#endif

static int	acl_special(struct archive_acl *acl,
		    int type, int permset, int tag);
static struct archive_acl_entry *acl_new_entry(struct archive_acl *acl,
		    int type, int permset, int tag, int id);
static int	archive_acl_add_entry_len_l(struct archive_acl *acl,
		    int type, int permset, int tag, int id, const char *name,
		    size_t len, struct archive_string_conv *sc);
static int	isint_w(const wchar_t *start, const wchar_t *end, int *result);
static int	ismode_w(const wchar_t *start, const wchar_t *end, int *result);
static void	next_field_w(const wchar_t **wp, const wchar_t **start,
		    const wchar_t **end, wchar_t *sep);
static int	prefix_w(const wchar_t *start, const wchar_t *end,
		    const wchar_t *test);
static void	append_entry_w(wchar_t **wp, const wchar_t *prefix, int tag,
		    const wchar_t *wname, int perm, int id);
static void	append_id_w(wchar_t **wp, int id);
static int	isint(const char *start, const char *end, int *result);
static int	ismode(const char *start, const char *end, int *result);
static void	next_field(const char **p, const char **start,
		    const char **end, char *sep);
static int	prefix_c(const char *start, const char *end,
		    const char *test);
static void	append_entry(char **p, const char *prefix, int tag,
		    const char *name, int perm, int id);
static void	append_id(char **p, int id);

void
archive_acl_clear(struct archive_acl *acl)
{
	struct archive_acl_entry *ap;

	while (acl->acl_head != NULL) {
		ap = acl->acl_head->next;
		archive_mstring_clean(&acl->acl_head->name);
		free(acl->acl_head);
		acl->acl_head = ap;
	}
	if (acl->acl_text_w != NULL) {
		free(acl->acl_text_w);
		acl->acl_text_w = NULL;
	}
	if (acl->acl_text != NULL) {
		free(acl->acl_text);
		acl->acl_text = NULL;
	}
	acl->acl_p = NULL;
	acl->acl_state = 0; /* Not counting. */
}

void
archive_acl_copy(struct archive_acl *dest, struct archive_acl *src)
{
	struct archive_acl_entry *ap, *ap2;

	archive_acl_clear(dest);

	dest->mode = src->mode;
	ap = src->acl_head;
	while (ap != NULL) {
		ap2 = acl_new_entry(dest,
		    ap->type, ap->permset, ap->tag, ap->id);
		if (ap2 != NULL)
			archive_mstring_copy(&ap2->name, &ap->name);
		ap = ap->next;
	}
}

int
archive_acl_add_entry(struct archive_acl *acl,
    int type, int permset, int tag, int id, const char *name)
{
	struct archive_acl_entry *ap;

	if (acl_special(acl, type, permset, tag) == 0)
		return ARCHIVE_OK;
	ap = acl_new_entry(acl, type, permset, tag, id);
	if (ap == NULL) {
		/* XXX Error XXX */
		return ARCHIVE_FAILED;
	}
	if (name != NULL  &&  *name != '\0')
		archive_mstring_copy_mbs(&ap->name, name);
	else
		archive_mstring_clean(&ap->name);
	return ARCHIVE_OK;
}

int
archive_acl_add_entry_w_len(struct archive_acl *acl,
    int type, int permset, int tag, int id, const wchar_t *name, size_t len)
{
	struct archive_acl_entry *ap;

	if (acl_special(acl, type, permset, tag) == 0)
		return ARCHIVE_OK;
	ap = acl_new_entry(acl, type, permset, tag, id);
	if (ap == NULL) {
		/* XXX Error XXX */
		return ARCHIVE_FAILED;
	}
	if (name != NULL  &&  *name != L'\0' && len > 0)
		archive_mstring_copy_wcs_len(&ap->name, name, len);
	else
		archive_mstring_clean(&ap->name);
	return ARCHIVE_OK;
}

static int
archive_acl_add_entry_len_l(struct archive_acl *acl,
    int type, int permset, int tag, int id, const char *name, size_t len,
    struct archive_string_conv *sc)
{
	struct archive_acl_entry *ap;
	int r;

	if (acl_special(acl, type, permset, tag) == 0)
		return ARCHIVE_OK;
	ap = acl_new_entry(acl, type, permset, tag, id);
	if (ap == NULL) {
		/* XXX Error XXX */
		return ARCHIVE_FAILED;
	}
	if (name != NULL  &&  *name != '\0' && len > 0) {
		r = archive_mstring_copy_mbs_len_l(&ap->name, name, len, sc);
	} else {
		r = 0;
		archive_mstring_clean(&ap->name);
	}
	if (r == 0)
		return (ARCHIVE_OK);
	else if (errno == ENOMEM)
		return (ARCHIVE_FATAL);
	else
		return (ARCHIVE_WARN);
}

/*
 * If this ACL entry is part of the standard POSIX permissions set,
 * store the permissions in the stat structure and return zero.
 */
static int
acl_special(struct archive_acl *acl, int type, int permset, int tag)
{
	if (type == ARCHIVE_ENTRY_ACL_TYPE_ACCESS
	    && ((permset & ~007) == 0)) {
		switch (tag) {
		case ARCHIVE_ENTRY_ACL_USER_OBJ:
			acl->mode &= ~0700;
			acl->mode |= (permset & 7) << 6;
			return (0);
		case ARCHIVE_ENTRY_ACL_GROUP_OBJ:
			acl->mode &= ~0070;
			acl->mode |= (permset & 7) << 3;
			return (0);
		case ARCHIVE_ENTRY_ACL_OTHER:
			acl->mode &= ~0007;
			acl->mode |= permset & 7;
			return (0);
		}
	}
	return (1);
}

/*
 * Allocate and populate a new ACL entry with everything but the
 * name.
 */
static struct archive_acl_entry *
acl_new_entry(struct archive_acl *acl,
    int type, int permset, int tag, int id)
{
	struct archive_acl_entry *ap, *aq;

	/* Type argument must be a valid NFS4 or POSIX.1e type.
	 * The type must agree with anything already set and
	 * the permset must be compatible. */
	if (type & ARCHIVE_ENTRY_ACL_TYPE_NFS4) {
		if (acl->acl_types & ~ARCHIVE_ENTRY_ACL_TYPE_NFS4) {
			return (NULL);
		}
		if (permset &
		    ~(ARCHIVE_ENTRY_ACL_PERMS_NFS4
			| ARCHIVE_ENTRY_ACL_INHERITANCE_NFS4)) {
			return (NULL);
		}
	} else	if (type & ARCHIVE_ENTRY_ACL_TYPE_POSIX1E) {
		if (acl->acl_types & ~ARCHIVE_ENTRY_ACL_TYPE_POSIX1E) {
			return (NULL);
		}
		if (permset & ~ARCHIVE_ENTRY_ACL_PERMS_POSIX1E) {
			return (NULL);
		}
	} else {
		return (NULL);
	}

	/* Verify the tag is valid and compatible with NFS4 or POSIX.1e. */
	switch (tag) {
	case ARCHIVE_ENTRY_ACL_USER:
	case ARCHIVE_ENTRY_ACL_USER_OBJ:
	case ARCHIVE_ENTRY_ACL_GROUP:
	case ARCHIVE_ENTRY_ACL_GROUP_OBJ:
		/* Tags valid in both NFS4 and POSIX.1e */
		break;
	case ARCHIVE_ENTRY_ACL_MASK:
	case ARCHIVE_ENTRY_ACL_OTHER:
		/* Tags valid only in POSIX.1e. */
		if (type & ~ARCHIVE_ENTRY_ACL_TYPE_POSIX1E) {
			return (NULL);
		}
		break;
	case ARCHIVE_ENTRY_ACL_EVERYONE:
		/* Tags valid only in NFS4. */
		if (type & ~ARCHIVE_ENTRY_ACL_TYPE_NFS4) {
			return (NULL);
		}
		break;
	default:
		/* No other values are valid. */
		return (NULL);
	}

	if (acl->acl_text_w != NULL) {
		free(acl->acl_text_w);
		acl->acl_text_w = NULL;
	}
	if (acl->acl_text != NULL) {
		free(acl->acl_text);
		acl->acl_text = NULL;
	}

	/* If there's a matching entry already in the list, overwrite it. */
	ap = acl->acl_head;
	aq = NULL;
	while (ap != NULL) {
		if (ap->type == type && ap->tag == tag && ap->id == id) {
			ap->permset = permset;
			return (ap);
		}
		aq = ap;
		ap = ap->next;
	}

	/* Add a new entry to the end of the list. */
	ap = (struct archive_acl_entry *)malloc(sizeof(*ap));
	if (ap == NULL)
		return (NULL);
	memset(ap, 0, sizeof(*ap));
	if (aq == NULL)
		acl->acl_head = ap;
	else
		aq->next = ap;
	ap->type = type;
	ap->tag = tag;
	ap->id = id;
	ap->permset = permset;
	acl->acl_types |= type;
	return (ap);
}

/*
 * Return a count of entries matching "want_type".
 */
int
archive_acl_count(struct archive_acl *acl, int want_type)
{
	int count;
	struct archive_acl_entry *ap;

	count = 0;
	ap = acl->acl_head;
	while (ap != NULL) {
		if ((ap->type & want_type) != 0)
			count++;
		ap = ap->next;
	}

	if (count > 0 && ((want_type & ARCHIVE_ENTRY_ACL_TYPE_ACCESS) != 0))
		count += 3;
	return (count);
}

/*
 * Prepare for reading entries from the ACL data.  Returns a count
 * of entries matching "want_type", or zero if there are no
 * non-extended ACL entries of that type.
 */
int
archive_acl_reset(struct archive_acl *acl, int want_type)
{
	int count, cutoff;

	count = archive_acl_count(acl, want_type);

	/*
	 * If the only entries are the three standard ones,
	 * then don't return any ACL data.  (In this case,
	 * client can just use chmod(2) to set permissions.)
	 */
	if ((want_type & ARCHIVE_ENTRY_ACL_TYPE_ACCESS) != 0)
		cutoff = 3;
	else
		cutoff = 0;

	if (count > cutoff)
		acl->acl_state = ARCHIVE_ENTRY_ACL_USER_OBJ;
	else
		acl->acl_state = 0;
	acl->acl_p = acl->acl_head;
	return (count);
}


/*
 * Return the next ACL entry in the list.  Fake entries for the
 * standard permissions and include them in the returned list.
 */
int
archive_acl_next(struct archive *a, struct archive_acl *acl, int want_type, int *type,
    int *permset, int *tag, int *id, const char **name)
{
	*name = NULL;
	*id = -1;

	/*
	 * The acl_state is either zero (no entries available), -1
	 * (reading from list), or an entry type (retrieve that type
	 * from ae_stat.aest_mode).
	 */
	if (acl->acl_state == 0)
		return (ARCHIVE_WARN);

	/* The first three access entries are special. */
	if ((want_type & ARCHIVE_ENTRY_ACL_TYPE_ACCESS) != 0) {
		switch (acl->acl_state) {
		case ARCHIVE_ENTRY_ACL_USER_OBJ:
			*permset = (acl->mode >> 6) & 7;
			*type = ARCHIVE_ENTRY_ACL_TYPE_ACCESS;
			*tag = ARCHIVE_ENTRY_ACL_USER_OBJ;
			acl->acl_state = ARCHIVE_ENTRY_ACL_GROUP_OBJ;
			return (ARCHIVE_OK);
		case ARCHIVE_ENTRY_ACL_GROUP_OBJ:
			*permset = (acl->mode >> 3) & 7;
			*type = ARCHIVE_ENTRY_ACL_TYPE_ACCESS;
			*tag = ARCHIVE_ENTRY_ACL_GROUP_OBJ;
			acl->acl_state = ARCHIVE_ENTRY_ACL_OTHER;
			return (ARCHIVE_OK);
		case ARCHIVE_ENTRY_ACL_OTHER:
			*permset = acl->mode & 7;
			*type = ARCHIVE_ENTRY_ACL_TYPE_ACCESS;
			*tag = ARCHIVE_ENTRY_ACL_OTHER;
			acl->acl_state = -1;
			acl->acl_p = acl->acl_head;
			return (ARCHIVE_OK);
		default:
			break;
		}
	}

	while (acl->acl_p != NULL && (acl->acl_p->type & want_type) == 0)
		acl->acl_p = acl->acl_p->next;
	if (acl->acl_p == NULL) {
		acl->acl_state = 0;
		*type = 0;
		*permset = 0;
		*tag = 0;
		*id = -1;
		*name = NULL;
		return (ARCHIVE_EOF); /* End of ACL entries. */
	}
	*type = acl->acl_p->type;
	*permset = acl->acl_p->permset;
	*tag = acl->acl_p->tag;
	*id = acl->acl_p->id;
	if (archive_mstring_get_mbs(a, &acl->acl_p->name, name) != 0) {
		if (errno == ENOMEM)
			return (ARCHIVE_FATAL);
		*name = NULL;
	}
	acl->acl_p = acl->acl_p->next;
	return (ARCHIVE_OK);
}

/*
 * Generate a text version of the ACL.  The flags parameter controls
 * the style of the generated ACL.
 */
const wchar_t *
archive_acl_text_w(struct archive *a, struct archive_acl *acl, int flags)
{
	int count;
	size_t length;
	const wchar_t *wname;
	const wchar_t *prefix;
	wchar_t separator;
	struct archive_acl_entry *ap;
	int id, r;
	wchar_t *wp;

	if (acl->acl_text_w != NULL) {
		free (acl->acl_text_w);
		acl->acl_text_w = NULL;
	}

	separator = L',';
	count = 0;
	length = 0;
	ap = acl->acl_head;
	while (ap != NULL) {
		if ((ap->type & flags) != 0) {
			count++;
			if ((flags & ARCHIVE_ENTRY_ACL_STYLE_MARK_DEFAULT) &&
			    (ap->type & ARCHIVE_ENTRY_ACL_TYPE_DEFAULT))
				length += 8; /* "default:" */
			length += 5; /* tag name */
			length += 1; /* colon */
			r = archive_mstring_get_wcs(a, &ap->name, &wname);
			if (r == 0 && wname != NULL)
				length += wcslen(wname);
			else if (r < 0 && errno == ENOMEM)
				return (NULL);
			else
				length += sizeof(uid_t) * 3 + 1;
			length ++; /* colon */
			length += 3; /* rwx */
			length += 1; /* colon */
			length += max(sizeof(uid_t), sizeof(gid_t)) * 3 + 1;
			length ++; /* newline */
		}
		ap = ap->next;
	}

	if (count > 0 && ((flags & ARCHIVE_ENTRY_ACL_TYPE_ACCESS) != 0)) {
		length += 10; /* "user::rwx\n" */
		length += 11; /* "group::rwx\n" */
		length += 11; /* "other::rwx\n" */
	}

	if (count == 0)
		return (NULL);

	/* Now, allocate the string and actually populate it. */
	wp = acl->acl_text_w = (wchar_t *)malloc(length * sizeof(wchar_t));
	if (wp == NULL)
		return (NULL);
	count = 0;
	if ((flags & ARCHIVE_ENTRY_ACL_TYPE_ACCESS) != 0) {
		append_entry_w(&wp, NULL, ARCHIVE_ENTRY_ACL_USER_OBJ, NULL,
		    acl->mode & 0700, -1);
		*wp++ = ',';
		append_entry_w(&wp, NULL, ARCHIVE_ENTRY_ACL_GROUP_OBJ, NULL,
		    acl->mode & 0070, -1);
		*wp++ = ',';
		append_entry_w(&wp, NULL, ARCHIVE_ENTRY_ACL_OTHER, NULL,
		    acl->mode & 0007, -1);
		count += 3;

		ap = acl->acl_head;
		while (ap != NULL) {
			if ((ap->type & ARCHIVE_ENTRY_ACL_TYPE_ACCESS) != 0) {
				r = archive_mstring_get_wcs(a, &ap->name, &wname);
				if (r == 0) {
					*wp++ = separator;
					if (flags & ARCHIVE_ENTRY_ACL_STYLE_EXTRA_ID)
						id = ap->id;
					else
						id = -1;
					append_entry_w(&wp, NULL, ap->tag, wname,
					    ap->permset, id);
					count++;
				} else if (r < 0 && errno == ENOMEM)
					return (NULL);
			}
			ap = ap->next;
		}
	}


	if ((flags & ARCHIVE_ENTRY_ACL_TYPE_DEFAULT) != 0) {
		if (flags & ARCHIVE_ENTRY_ACL_STYLE_MARK_DEFAULT)
			prefix = L"default:";
		else
			prefix = NULL;
		ap = acl->acl_head;
		count = 0;
		while (ap != NULL) {
			if ((ap->type & ARCHIVE_ENTRY_ACL_TYPE_DEFAULT) != 0) {
				r = archive_mstring_get_wcs(a, &ap->name, &wname);
				if (r == 0) {
					if (count > 0)
						*wp++ = separator;
					if (flags & ARCHIVE_ENTRY_ACL_STYLE_EXTRA_ID)
						id = ap->id;
					else
						id = -1;
					append_entry_w(&wp, prefix, ap->tag,
					    wname, ap->permset, id);
					count ++;
				} else if (r < 0 && errno == ENOMEM)
					return (NULL);
			}
			ap = ap->next;
		}
	}

	return (acl->acl_text_w);
}


static void
append_id_w(wchar_t **wp, int id)
{
	if (id < 0)
		id = 0;
	if (id > 9)
		append_id_w(wp, id / 10);
	*(*wp)++ = L"0123456789"[id % 10];
}

static void
append_entry_w(wchar_t **wp, const wchar_t *prefix, int tag,
    const wchar_t *wname, int perm, int id)
{
	if (prefix != NULL) {
		wcscpy(*wp, prefix);
		*wp += wcslen(*wp);
	}
	switch (tag) {
	case ARCHIVE_ENTRY_ACL_USER_OBJ:
		wname = NULL;
		id = -1;
		/* FALLTHROUGH */
	case ARCHIVE_ENTRY_ACL_USER:
		wcscpy(*wp, L"user");
		break;
	case ARCHIVE_ENTRY_ACL_GROUP_OBJ:
		wname = NULL;
		id = -1;
		/* FALLTHROUGH */
	case ARCHIVE_ENTRY_ACL_GROUP:
		wcscpy(*wp, L"group");
		break;
	case ARCHIVE_ENTRY_ACL_MASK:
		wcscpy(*wp, L"mask");
		wname = NULL;
		id = -1;
		break;
	case ARCHIVE_ENTRY_ACL_OTHER:
		wcscpy(*wp, L"other");
		wname = NULL;
		id = -1;
		break;
	}
	*wp += wcslen(*wp);
	*(*wp)++ = L':';
	if (wname != NULL) {
		wcscpy(*wp, wname);
		*wp += wcslen(*wp);
	} else if (tag == ARCHIVE_ENTRY_ACL_USER
	    || tag == ARCHIVE_ENTRY_ACL_GROUP) {
		append_id_w(wp, id);
		id = -1;
	}
	*(*wp)++ = L':';
	*(*wp)++ = (perm & 0444) ? L'r' : L'-';
	*(*wp)++ = (perm & 0222) ? L'w' : L'-';
	*(*wp)++ = (perm & 0111) ? L'x' : L'-';
	if (id != -1) {
		*(*wp)++ = L':';
		append_id_w(wp, id);
	}
	**wp = L'\0';
}

int
archive_acl_text_l(struct archive_acl *acl, int flags,
    const char **acl_text, size_t *acl_text_len,
    struct archive_string_conv *sc)
{
	int count;
	size_t length;
	const char *name;
	const char *prefix;
	char separator;
	struct archive_acl_entry *ap;
	size_t len;
	int id, r;
	char *p;

	if (acl->acl_text != NULL) {
		free (acl->acl_text);
		acl->acl_text = NULL;
	}

	*acl_text = NULL;
	if (acl_text_len != NULL)
		*acl_text_len = 0;
	separator = ',';
	count = 0;
	length = 0;
	ap = acl->acl_head;
	while (ap != NULL) {
		if ((ap->type & flags) != 0) {
			count++;
			if ((flags & ARCHIVE_ENTRY_ACL_STYLE_MARK_DEFAULT) &&
			    (ap->type & ARCHIVE_ENTRY_ACL_TYPE_DEFAULT))
				length += 8; /* "default:" */
			length += 5; /* tag name */
			length += 1; /* colon */
			r = archive_mstring_get_mbs_l(
			    &ap->name, &name, &len, sc);
			if (r != 0)
				return (-1);
			if (len > 0 && name != NULL)
				length += len;
			else
				length += sizeof(uid_t) * 3 + 1;
			length ++; /* colon */
			length += 3; /* rwx */
			length += 1; /* colon */
			length += max(sizeof(uid_t), sizeof(gid_t)) * 3 + 1;
			length ++; /* newline */
		}
		ap = ap->next;
	}

	if (count > 0 && ((flags & ARCHIVE_ENTRY_ACL_TYPE_ACCESS) != 0)) {
		length += 10; /* "user::rwx\n" */
		length += 11; /* "group::rwx\n" */
		length += 11; /* "other::rwx\n" */
	}

	if (count == 0)
		return (0);

	/* Now, allocate the string and actually populate it. */
	p = acl->acl_text = (char *)malloc(length);
	if (p == NULL)
		return (-1);
	count = 0;
	if ((flags & ARCHIVE_ENTRY_ACL_TYPE_ACCESS) != 0) {
		append_entry(&p, NULL, ARCHIVE_ENTRY_ACL_USER_OBJ, NULL,
		    acl->mode & 0700, -1);
		*p++ = ',';
		append_entry(&p, NULL, ARCHIVE_ENTRY_ACL_GROUP_OBJ, NULL,
		    acl->mode & 0070, -1);
		*p++ = ',';
		append_entry(&p, NULL, ARCHIVE_ENTRY_ACL_OTHER, NULL,
		    acl->mode & 0007, -1);
		count += 3;

		for (ap = acl->acl_head; ap != NULL; ap = ap->next) {
			if ((ap->type & ARCHIVE_ENTRY_ACL_TYPE_ACCESS) == 0)
				continue;
			r = archive_mstring_get_mbs_l(
			    &ap->name, &name, &len, sc);
			if (r != 0)
				return (-1);
			*p++ = separator;
			if (flags & ARCHIVE_ENTRY_ACL_STYLE_EXTRA_ID)
				id = ap->id;
			else
				id = -1;
			append_entry(&p, NULL, ap->tag, name,
			    ap->permset, id);
			count++;
		}
	}


	if ((flags & ARCHIVE_ENTRY_ACL_TYPE_DEFAULT) != 0) {
		if (flags & ARCHIVE_ENTRY_ACL_STYLE_MARK_DEFAULT)
			prefix = "default:";
		else
			prefix = NULL;
		count = 0;
		for (ap = acl->acl_head; ap != NULL; ap = ap->next) {
			if ((ap->type & ARCHIVE_ENTRY_ACL_TYPE_DEFAULT) == 0)
				continue;
			r = archive_mstring_get_mbs_l(
			    &ap->name, &name, &len, sc);
			if (r != 0)
				return (-1);
			if (count > 0)
				*p++ = separator;
			if (flags & ARCHIVE_ENTRY_ACL_STYLE_EXTRA_ID)
				id = ap->id;
			else
				id = -1;
			append_entry(&p, prefix, ap->tag,
			    name, ap->permset, id);
			count ++;
		}
	}

	*acl_text = acl->acl_text;
	if (acl_text_len != NULL)
		*acl_text_len = strlen(acl->acl_text);
	return (0);
}

static void
append_id(char **p, int id)
{
	if (id < 0)
		id = 0;
	if (id > 9)
		append_id(p, id / 10);
	*(*p)++ = "0123456789"[id % 10];
}

static void
append_entry(char **p, const char *prefix, int tag,
    const char *name, int perm, int id)
{
	if (prefix != NULL) {
		strcpy(*p, prefix);
		*p += strlen(*p);
	}
	switch (tag) {
	case ARCHIVE_ENTRY_ACL_USER_OBJ:
		name = NULL;
		id = -1;
		/* FALLTHROUGH */
	case ARCHIVE_ENTRY_ACL_USER:
		strcpy(*p, "user");
		break;
	case ARCHIVE_ENTRY_ACL_GROUP_OBJ:
		name = NULL;
		id = -1;
		/* FALLTHROUGH */
	case ARCHIVE_ENTRY_ACL_GROUP:
		strcpy(*p, "group");
		break;
	case ARCHIVE_ENTRY_ACL_MASK:
		strcpy(*p, "mask");
		name = NULL;
		id = -1;
		break;
	case ARCHIVE_ENTRY_ACL_OTHER:
		strcpy(*p, "other");
		name = NULL;
		id = -1;
		break;
	}
	*p += strlen(*p);
	*(*p)++ = ':';
	if (name != NULL) {
		strcpy(*p, name);
		*p += strlen(*p);
	} else if (tag == ARCHIVE_ENTRY_ACL_USER
	    || tag == ARCHIVE_ENTRY_ACL_GROUP) {
		append_id(p, id);
		id = -1;
	}
	*(*p)++ = ':';
	*(*p)++ = (perm & 0444) ? 'r' : '-';
	*(*p)++ = (perm & 0222) ? 'w' : '-';
	*(*p)++ = (perm & 0111) ? 'x' : '-';
	if (id != -1) {
		*(*p)++ = ':';
		append_id(p, id);
	}
	**p = '\0';
}

/*
 * Parse a textual ACL.  This automatically recognizes and supports
 * extensions described above.  The 'type' argument is used to
 * indicate the type that should be used for any entries not
 * explicitly marked as "default:".
 */
int
archive_acl_parse_w(struct archive_acl *acl,
    const wchar_t *text, int default_type)
{
	struct {
		const wchar_t *start;
		const wchar_t *end;
	} field[4], name;

	int fields, n;
	int type, tag, permset, id;
	wchar_t sep;

	while (text != NULL  &&  *text != L'\0') {
		/*
		 * Parse the fields out of the next entry,
		 * advance 'text' to start of next entry.
		 */
		fields = 0;
		do {
			const wchar_t *start, *end;
			next_field_w(&text, &start, &end, &sep);
			if (fields < 4) {
				field[fields].start = start;
				field[fields].end = end;
			}
			++fields;
		} while (sep == L':');

		/* Set remaining fields to blank. */
		for (n = fields; n < 4; ++n)
			field[n].start = field[n].end = NULL;

		/* Check for a numeric ID in field 1 or 3. */
		id = -1;
		isint_w(field[1].start, field[1].end, &id);
		/* Field 3 is optional. */
		if (id == -1 && fields > 3)
			isint_w(field[3].start, field[3].end, &id);

		/*
		 * Solaris extension:  "defaultuser::rwx" is the
		 * default ACL corresponding to "user::rwx", etc.
		 */
		if (field[0].end - field[0].start > 7
		    && wmemcmp(field[0].start, L"default", 7) == 0) {
			type = ARCHIVE_ENTRY_ACL_TYPE_DEFAULT;
			field[0].start += 7;
		} else
			type = default_type;

		name.start = name.end = NULL;
		if (prefix_w(field[0].start, field[0].end, L"user")) {
			if (!ismode_w(field[2].start, field[2].end, &permset))
				return (ARCHIVE_WARN);
			if (id != -1 || field[1].start < field[1].end) {
				tag = ARCHIVE_ENTRY_ACL_USER;
				name = field[1];
			} else
				tag = ARCHIVE_ENTRY_ACL_USER_OBJ;
		} else if (prefix_w(field[0].start, field[0].end, L"group")) {
			if (!ismode_w(field[2].start, field[2].end, &permset))
				return (ARCHIVE_WARN);
			if (id != -1 || field[1].start < field[1].end) {
				tag = ARCHIVE_ENTRY_ACL_GROUP;
				name = field[1];
			} else
				tag = ARCHIVE_ENTRY_ACL_GROUP_OBJ;
		} else if (prefix_w(field[0].start, field[0].end, L"other")) {
			if (fields == 2
			    && field[1].start < field[1].end
			    && ismode_w(field[1].start, field[1].end, &permset)) {
				/* This is Solaris-style "other:rwx" */
			} else if (fields == 3
			    && field[1].start == field[1].end
			    && field[2].start < field[2].end
			    && ismode_w(field[2].start, field[2].end, &permset)) {
				/* This is FreeBSD-style "other::rwx" */
			} else
				return (ARCHIVE_WARN);
			tag = ARCHIVE_ENTRY_ACL_OTHER;
		} else if (prefix_w(field[0].start, field[0].end, L"mask")) {
			if (fields == 2
			    && field[1].start < field[1].end
			    && ismode_w(field[1].start, field[1].end, &permset)) {
				/* This is Solaris-style "mask:rwx" */
			} else if (fields == 3
			    && field[1].start == field[1].end
			    && field[2].start < field[2].end
			    && ismode_w(field[2].start, field[2].end, &permset)) {
				/* This is FreeBSD-style "mask::rwx" */
			} else
				return (ARCHIVE_WARN);
			tag = ARCHIVE_ENTRY_ACL_MASK;
		} else
			return (ARCHIVE_WARN);

		/* Add entry to the internal list. */
		archive_acl_add_entry_w_len(acl, type, permset,
		    tag, id, name.start, name.end - name.start);
	}
	return (ARCHIVE_OK);
}

/*
 * Parse a string to a positive decimal integer.  Returns true if
 * the string is non-empty and consists only of decimal digits,
 * false otherwise.
 */
static int
isint_w(const wchar_t *start, const wchar_t *end, int *result)
{
	int n = 0;
	if (start >= end)
		return (0);
	while (start < end) {
		if (*start < '0' || *start > '9')
			return (0);
		if (n > (INT_MAX / 10) ||
		    (n == INT_MAX / 10 && (*start - '0') > INT_MAX % 10)) {
			n = INT_MAX;
		} else {
			n *= 10;
			n += *start - '0';
		}
		start++;
	}
	*result = n;
	return (1);
}

/*
 * Parse a string as a mode field.  Returns true if
 * the string is non-empty and consists only of mode characters,
 * false otherwise.
 */
static int
ismode_w(const wchar_t *start, const wchar_t *end, int *permset)
{
	const wchar_t *p;

	if (start >= end)
		return (0);
	p = start;
	*permset = 0;
	while (p < end) {
		switch (*p++) {
		case 'r': case 'R':
			*permset |= ARCHIVE_ENTRY_ACL_READ;
			break;
		case 'w': case 'W':
			*permset |= ARCHIVE_ENTRY_ACL_WRITE;
			break;
		case 'x': case 'X':
			*permset |= ARCHIVE_ENTRY_ACL_EXECUTE;
			break;
		case '-':
			break;
		default:
			return (0);
		}
	}
	return (1);
}

/*
 * Match "[:whitespace:]*(.*)[:whitespace:]*[:,\n]".  *wp is updated
 * to point to just after the separator.  *start points to the first
 * character of the matched text and *end just after the last
 * character of the matched identifier.  In particular *end - *start
 * is the length of the field body, not including leading or trailing
 * whitespace.
 */
static void
next_field_w(const wchar_t **wp, const wchar_t **start,
    const wchar_t **end, wchar_t *sep)
{
	/* Skip leading whitespace to find start of field. */
	while (**wp == L' ' || **wp == L'\t' || **wp == L'\n') {
		(*wp)++;
	}
	*start = *wp;

	/* Scan for the separator. */
	while (**wp != L'\0' && **wp != L',' && **wp != L':' &&
	    **wp != L'\n') {
		(*wp)++;
	}
	*sep = **wp;

	/* Trim trailing whitespace to locate end of field. */
	*end = *wp - 1;
	while (**end == L' ' || **end == L'\t' || **end == L'\n') {
		(*end)--;
	}
	(*end)++;

	/* Adjust scanner location. */
	if (**wp != L'\0')
		(*wp)++;
}

/*
 * Return true if the characters [start...end) are a prefix of 'test'.
 * This makes it easy to handle the obvious abbreviations: 'u' for 'user', etc.
 */
static int
prefix_w(const wchar_t *start, const wchar_t *end, const wchar_t *test)
{
	if (start == end)
		return (0);

	if (*start++ != *test++)
		return (0);

	while (start < end  &&  *start++ == *test++)
		;

	if (start < end)
		return (0);

	return (1);
}

/*
 * Parse a textual ACL.  This automatically recognizes and supports
 * extensions described above.  The 'type' argument is used to
 * indicate the type that should be used for any entries not
 * explicitly marked as "default:".
 */
int
archive_acl_parse_l(struct archive_acl *acl,
    const char *text, int default_type, struct archive_string_conv *sc)
{
	struct {
		const char *start;
		const char *end;
	} field[4], name;

	int fields, n, r, ret = ARCHIVE_OK;
	int type, tag, permset, id;
	char sep;

	while (text != NULL  &&  *text != '\0') {
		/*
		 * Parse the fields out of the next entry,
		 * advance 'text' to start of next entry.
		 */
		fields = 0;
		do {
			const char *start, *end;
			next_field(&text, &start, &end, &sep);
			if (fields < 4) {
				field[fields].start = start;
				field[fields].end = end;
			}
			++fields;
		} while (sep == ':');

		/* Set remaining fields to blank. */
		for (n = fields; n < 4; ++n)
			field[n].start = field[n].end = NULL;

		/* Check for a numeric ID in field 1 or 3. */
		id = -1;
		isint(field[1].start, field[1].end, &id);
		/* Field 3 is optional. */
		if (id == -1 && fields > 3)
			isint(field[3].start, field[3].end, &id);

		/*
		 * Solaris extension:  "defaultuser::rwx" is the
		 * default ACL corresponding to "user::rwx", etc.
		 */
		if (field[0].end - field[0].start > 7
		    && memcmp(field[0].start, "default", 7) == 0) {
			type = ARCHIVE_ENTRY_ACL_TYPE_DEFAULT;
			field[0].start += 7;
		} else
			type = default_type;

		name.start = name.end = NULL;
		if (prefix_c(field[0].start, field[0].end, "user")) {
			if (!ismode(field[2].start, field[2].end, &permset))
				return (ARCHIVE_WARN);
			if (id != -1 || field[1].start < field[1].end) {
				tag = ARCHIVE_ENTRY_ACL_USER;
				name = field[1];
			} else
				tag = ARCHIVE_ENTRY_ACL_USER_OBJ;
		} else if (prefix_c(field[0].start, field[0].end, "group")) {
			if (!ismode(field[2].start, field[2].end, &permset))
				return (ARCHIVE_WARN);
			if (id != -1 || field[1].start < field[1].end) {
				tag = ARCHIVE_ENTRY_ACL_GROUP;
				name = field[1];
			} else
				tag = ARCHIVE_ENTRY_ACL_GROUP_OBJ;
		} else if (prefix_c(field[0].start, field[0].end, "other")) {
			if (fields == 2
			    && field[1].start < field[1].end
			    && ismode(field[1].start, field[1].end, &permset)) {
				/* This is Solaris-style "other:rwx" */
			} else if (fields == 3
			    && field[1].start == field[1].end
			    && field[2].start < field[2].end
			    && ismode(field[2].start, field[2].end, &permset)) {
				/* This is FreeBSD-style "other::rwx" */
			} else
				return (ARCHIVE_WARN);
			tag = ARCHIVE_ENTRY_ACL_OTHER;
		} else if (prefix_c(field[0].start, field[0].end, "mask")) {
			if (fields == 2
			    && field[1].start < field[1].end
			    && ismode(field[1].start, field[1].end, &permset)) {
				/* This is Solaris-style "mask:rwx" */
			} else if (fields == 3
			    && field[1].start == field[1].end
			    && field[2].start < field[2].end
			    && ismode(field[2].start, field[2].end, &permset)) {
				/* This is FreeBSD-style "mask::rwx" */
			} else
				return (ARCHIVE_WARN);
			tag = ARCHIVE_ENTRY_ACL_MASK;
		} else
			return (ARCHIVE_WARN);

		/* Add entry to the internal list. */
		r = archive_acl_add_entry_len_l(acl, type, permset,
		    tag, id, name.start, name.end - name.start, sc);
		if (r < ARCHIVE_WARN)
			return (r);
		if (r != ARCHIVE_OK)
			ret = ARCHIVE_WARN;
	}
	return (ret);
}

/*
 * Parse a string to a positive decimal integer.  Returns true if
 * the string is non-empty and consists only of decimal digits,
 * false otherwise.
 */
static int
isint(const char *start, const char *end, int *result)
{
	int n = 0;
	if (start >= end)
		return (0);
	while (start < end) {
		if (*start < '0' || *start > '9')
			return (0);
		if (n > (INT_MAX / 10) ||
		    (n == INT_MAX / 10 && (*start - '0') > INT_MAX % 10)) {
			n = INT_MAX;
		} else {
			n *= 10;
			n += *start - '0';
		}
		start++;
	}
	*result = n;
	return (1);
}

/*
 * Parse a string as a mode field.  Returns true if
 * the string is non-empty and consists only of mode characters,
 * false otherwise.
 */
static int
ismode(const char *start, const char *end, int *permset)
{
	const char *p;

	if (start >= end)
		return (0);
	p = start;
	*permset = 0;
	while (p < end) {
		switch (*p++) {
		case 'r': case 'R':
			*permset |= ARCHIVE_ENTRY_ACL_READ;
			break;
		case 'w': case 'W':
			*permset |= ARCHIVE_ENTRY_ACL_WRITE;
			break;
		case 'x': case 'X':
			*permset |= ARCHIVE_ENTRY_ACL_EXECUTE;
			break;
		case '-':
			break;
		default:
			return (0);
		}
	}
	return (1);
}

/*
 * Match "[:whitespace:]*(.*)[:whitespace:]*[:,\n]".  *wp is updated
 * to point to just after the separator.  *start points to the first
 * character of the matched text and *end just after the last
 * character of the matched identifier.  In particular *end - *start
 * is the length of the field body, not including leading or trailing
 * whitespace.
 */
static void
next_field(const char **p, const char **start,
    const char **end, char *sep)
{
	/* Skip leading whitespace to find start of field. */
	while (**p == ' ' || **p == '\t' || **p == '\n') {
		(*p)++;
	}
	*start = *p;

	/* Scan for the separator. */
	while (**p != '\0' && **p != ',' && **p != ':' && **p != '\n') {
		(*p)++;
	}
	*sep = **p;

	/* Trim trailing whitespace to locate end of field. */
	*end = *p - 1;
	while (**end == ' ' || **end == '\t' || **end == '\n') {
		(*end)--;
	}
	(*end)++;

	/* Adjust scanner location. */
	if (**p != '\0')
		(*p)++;
}

/*
 * Return true if the characters [start...end) are a prefix of 'test'.
 * This makes it easy to handle the obvious abbreviations: 'u' for 'user', etc.
 */
static int
prefix_c(const char *start, const char *end, const char *test)
{
	if (start == end)
		return (0);

	if (*start++ != *test++)
		return (0);

	while (start < end  &&  *start++ == *test++)
		;

	if (start < end)
		return (0);

	return (1);
}
