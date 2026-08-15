// Copyright (C) 2022-2024 Artifex Software, Inc.
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

public class Story
{
	static {
		Context.init();
	}

	/* Flags for 3-args place() */
	public static final int FLAGS_NO_OVERFLOW = 1;

	/* Return codes from 3-args place().
	 * Also 'non-zero' means more to do.
	 */
	public static final int ALL_FITTED = 0;
	public static final int OVERFLOW_WIDTH = 2;

	protected long pointer;

	protected native void finalize();

	public void destroy() {
		finalize();
	}

	private static native long newStory(byte[] content, byte[] user_css, float em, Archive arch);

	public Story(String content, String user_css, float em, Archive arch)
	{
		pointer = newStory(content.getBytes(), user_css.getBytes(), em, arch);
	}
	public Story(String content, String user_css, float em)
	{
		this(content, user_css, em, null);
	}

	public Story(byte[] content, String user_css, float em, Archive arch)
	{
		pointer = newStory(content, user_css.getBytes(), em, arch);
	}
	public Story(byte[] content, String user_css, float em)
	{
		this(content, user_css, em, null);
	}

	public Story(String content, byte[] user_css, float em, Archive arch)
	{
		pointer = newStory(content.getBytes(), user_css, em, arch);
	}
	public Story(String content, byte[] user_css, float em)
	{
		this(content, user_css, em, null);
	}

	public Story(byte[] content, byte[] user_css, float em, Archive arch)
	{
		pointer = newStory(content, user_css, em, arch);
	}
	public Story(byte[] content, byte[] user_css, float em)
	{
		this(content, user_css, em, null);
	}

	public boolean place(Rect rect, Rect filled)
	{
		return place(rect, filled, 0) != 0;
	}

	public native int place(Rect rect, Rect filled, int flags);

	public native void draw(Device dev, Matrix ctm);

	public native DOM document();
}
