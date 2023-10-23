// Copyright (C) 2004-2021 Artifex Software, Inc.
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

	public int insert(OutlineItem item)
	{
		return insert(item.title, item.uri, item.is_open);
	}
	public native int insert(String title, String uri, boolean is_open);
	public void update(OutlineItem item)
	{
		update(item.title, item.uri, item.is_open);
	}
	public native void update(String title, String uri, boolean is_open);
	public native OutlineItem item();
	public native int delete();

	public static class OutlineItem {
		public String title;
		public String uri;
		public boolean is_open;

		public OutlineItem(String title, String uri, boolean is_open)
		{
			this.title = title;
			this.uri = uri;
			this.is_open = is_open;
		}
	}
}
