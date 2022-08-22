// Copyright (C) 2022 Artifex Software, Inc.
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
// Artifex Software, Inc., 1305 Grant Avenue - Suite 200, Novato,
// CA 94945, U.S.A., +1(415)492-9861, for further information.

package com.artifex.mupdf.fitz;

public class Story
{
	static {
		Context.init();
	}

	protected long pointer;

	protected native void finalize();

	public void destroy() {
		finalize();
	}

	private static native long newStory(byte[] content, byte[] user_css, float em);

	public Story(String content, String user_css, float em)
	{
		pointer = newStory(content.getBytes(), user_css.getBytes(), em);
	}

	public Story(byte[] content, String user_css, float em)
	{
		pointer = newStory(content, user_css.getBytes(), em);
	}

	public Story(String content, byte[] user_css, float em)
	{
		pointer = newStory(content.getBytes(), user_css, em);
	}

	public Story(byte[] content, byte[] user_css, float em)
	{
		pointer = newStory(content, user_css, em);
	}

	public native boolean place(Rect rect, Rect filled);

	public native void draw(Device dev, Matrix ctm);

	public native DOM document();
}
