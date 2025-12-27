// Copyright (C) 2004-2025 Artifex Software, Inc.
//
// This file is part of MuPDF.
//
// MuPDF is free software: you can redistribute it and/or modify it under the
// terms of the GNU Affero General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
//
// MuPDF is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more
// details.
//
// You should have received a copy of the GNU Affero General Public License
// along with MuPDF. If not, see <https://www.gnu.org/licenses/agpl-3.0.en.html>
//
// Alternative licensing terms are available from the licensor.
// For commercial licensing, see <https://www.artifex.com/> or contact
// Artifex Software, Inc., 39 Mesa Street, Suite 108A, San Francisco,
// CA 94129, USA, for further information.

package com.artifex.mupdf.fitz;

public class OutlineIterator
{
	protected long pointer;

	protected native void finalize();

	public void destroy() {
		finalize();
	}

	protected OutlineIterator(long p) {
		pointer = p;
	}

	public native int next();
	public native int prev();
	public native int up();
	public native int down();

	public native int insert(String title, String uri, boolean is_open, float r, float g, float b, int flags);
	public int insert(OutlineItem item)
	{
		return insert(item.title, item.uri, item.is_open, item.r, item.g, item.b, item.flags);
	}
	public int insert(String title, String uri, boolean is_open)
	{
		return insert(title, uri, is_open, 0, 0, 0, 0);
	}
	public native void update(String title, String uri, boolean is_open, float r, float g, float b, int flags);
	public void update(OutlineItem item)
	{
		update(item.title, item.uri, item.is_open, item.r, item.g, item.b, item.flags);
	}

	public native OutlineItem item();
	public native int delete();

	public static final int ITERATOR_DID_NOT_MOVE = -1;
	public static final int ITERATOR_AT_ITEM = 0;
	public static final int ITERATOR_AT_EMPTY = 1;

	public static final int FLAG_BOLD = 1;
	public static final int FLAG_ITALIC = 2;

	public static class OutlineItem {
		public String title;
		public String uri;
		public boolean is_open;
		public float r;
		public float g;
		public float b;
		public int flags;

		public OutlineItem(String title, String uri, boolean is_open, float r, float g, float b, int flags)
		{
			this.title = title;
			this.uri = uri;
			this.is_open = is_open;
			this.r = r;
			this.g = g;
			this.b = b;
			this.flags = flags;
		}
		public OutlineItem(String title, String uri, boolean is_open)
		{
			this.title = title;
			this.uri = uri;
			this.is_open = is_open;
			this.r = 0;
			this.g = 0;
			this.b = 0;
			this.flags = 0;
		}
	}
}
