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
// Artifex Software, Inc., 1305 Grant Avenue - Suite 200, Novato,
// CA 94945, U.S.A., +1(415)492-9861, for further information.

package com.artifex.mupdf.fitz;

public class Image
{
	static {
		Context.init();
	}

	protected long pointer;

	protected native void finalize();

	public void destroy() {
		finalize();
	}

	private native long newNativeFromPixmap(Pixmap pixmap);
	private native long newNativeFromFile(String filename);
	private native long newNativeFromBytes(byte[] bytes);

	protected Image(long p) {
		pointer = p;
	}

	public Image(Pixmap pixmap) {
		pointer = newNativeFromPixmap(pixmap);
	}

	public Image(String filename) {
		pointer = newNativeFromFile(filename);
	}

	public Image(byte[] bytes) {
		pointer = newNativeFromBytes(bytes);
	}

	public native int getWidth();
	public native int getHeight();
	public native int getXResolution();
	public native int getYResolution();

	public native ColorSpace getColorSpace();
	public native int getNumberOfComponents();
	public native int getBitsPerComponent();
	public native boolean getImageMask();
	public native boolean getInterpolate();
	public native Image getMask();

	public native Pixmap toPixmap();
}
