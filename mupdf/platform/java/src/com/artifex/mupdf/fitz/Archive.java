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

public class Archive
{
	static {
		Context.init();
	}

	private long pointer;

	protected native void finalize();

	public void destroy() {
		finalize();
	}

	private native long newNativeArchiveWithPath(String path);
	private native long newNativeArchiveWithStream(SeekableInputStream stream);

	public Archive(String path) {
		pointer = newNativeArchiveWithPath(path);
	}

	public Archive(SeekableInputStream stream) {
		pointer = newNativeArchiveWithStream(stream);
	}

	protected Archive(long p) {
		pointer = p;
	}

	public native String getFormat();
	public native int countEntries();
	public native String listEntry(int idx);
	public native boolean hasEntry(String name);
	public native Buffer readEntry(String name);
}
