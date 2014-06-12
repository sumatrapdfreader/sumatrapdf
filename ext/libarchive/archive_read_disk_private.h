/*-
 * Copyright (c) 2003-2009 Tim Kientzle
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
 *
 * $FreeBSD: head/lib/libarchive/archive_read_disk_private.h 201105 2009-12-28 03:20:54Z kientzle $
 */

#ifndef __LIBARCHIVE_BUILD
#error This header is only to be used internally to libarchive.
#endif

#ifndef ARCHIVE_READ_DISK_PRIVATE_H_INCLUDED
#define ARCHIVE_READ_DISK_PRIVATE_H_INCLUDED

struct tree;
struct archive_entry;

struct archive_read_disk {
	struct archive	archive;

	/*
	 * Symlink mode is one of 'L'ogical, 'P'hysical, or 'H'ybrid,
	 * following an old BSD convention.  'L' follows all symlinks,
	 * 'P' follows none, 'H' follows symlinks only for the first
	 * item.
	 */
	char	symlink_mode;

	/*
	 * Since symlink interaction changes, we need to track whether
	 * we're following symlinks for the current item.  'L' mode above
	 * sets this true, 'P' sets it false, 'H' changes it as we traverse.
	 */
	char	follow_symlinks;  /* Either 'L' or 'P'. */

	/* Directory traversals. */
	struct tree *tree;
	int	(*open_on_current_dir)(struct tree*, const char *, int);
	int	(*tree_current_dir_fd)(struct tree*);
	int	(*tree_enter_working_dir)(struct tree*);

	/* Set 1 if users request to restore atime . */
	int		 restore_time;
	/* Set 1 if users request to honor nodump flag . */
	int		 honor_nodump;
	/* Set 1 if users request to enable mac copyfile. */
	int		 enable_copyfile;
	/* Set 1 if users request to traverse mount points. */
	int		 traverse_mount_points;

	const char * (*lookup_gname)(void *private, int64_t gid);
	void	(*cleanup_gname)(void *private);
	void	 *lookup_gname_data;
	const char * (*lookup_uname)(void *private, int64_t uid);
	void	(*cleanup_uname)(void *private);
	void	 *lookup_uname_data;

	int	(*metadata_filter_func)(struct archive *, void *,
			struct archive_entry *);
	void	*metadata_filter_data;

	/* ARCHIVE_MATCH object. */
	struct archive	*matching;
	/* Callback function, this will be invoked when ARCHIVE_MATCH
	 * archive_match_*_excluded_ae return true. */
	void	(*excluded_cb_func)(struct archive *, void *,
			 struct archive_entry *);
	void	*excluded_cb_data;
};

#endif
