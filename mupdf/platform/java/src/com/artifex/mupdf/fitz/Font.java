// Copyright (C) 2004-2022 Artifex Software, Inc.
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

public class Font
{
	static {
		Context.init();
	}

	public static final int SIMPLE_ENCODING_LATIN = 0;
	public static final int SIMPLE_ENCODING_GREEK = 1;
	public static final int SIMPLE_ENCODING_CYRILLIC = 2;

	public static final int ADOBE_CNS = 0;
	public static final int ADOBE_GB = 1;
	public static final int ADOBE_JAPAN = 2;
	public static final int ADOBE_KOREA = 3;

	private long pointer;

	protected native void finalize();

	public void destroy() {
		finalize();
	}

	private native long newNative(String name, int index);

	private Font(long p) {
		pointer = p;
	}

	public Font(String name, int index) {
		pointer = newNative(name, index);
	}

	public Font(String name) {
		pointer = newNative(name, 0);
	}

	public native String getName();

	public native int encodeCharacter(int unicode);
	public native float advanceGlyph(int glyph, boolean wmode);

	public float advanceGlyph(int glyph) {
		return advanceGlyph(glyph, false);
	}

	public String toString() {
		return "Font(" + getName() + ")";
	}
}
