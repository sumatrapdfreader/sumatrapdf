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

public class Text implements TextWalker
{
	static {
		Context.init();
	}

	private long pointer;

	protected native void finalize();

	public void destroy() {
		finalize();
	}

	private native long newNative();

	private Text(long p) {
		pointer = p;
	}

	public Text() {
		pointer = newNative();
	}

	public native void showGlyph(Font font, Matrix trm, int glyph, int unicode, boolean wmode);
	public native void showString(Font font, Matrix trm, String str, boolean wmode);

	public native Rect getBounds(StrokeState stroke, Matrix ctm);

	public void showGlyph(Font font, Matrix trm, int glyph, int unicode) {
		showGlyph(font, trm, glyph, unicode, false);
	}

	public void showString(Font font, Matrix trm, String str) {
		showString(font, trm, str, false);
	}

	public native void walk(TextWalker walker);
}
