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

public class DisplayList
{
	static {
		Context.init();
	}

	private long pointer;

	protected native void finalize();

	public void destroy() {
		finalize();
	}

	private native long newNative(Rect mediabox);

	public DisplayList(Rect mediabox) {
		pointer = newNative(mediabox);
	}

	private DisplayList(long p) {
		pointer = p;
	}

	public native Rect getBounds();

	public native Pixmap toPixmap(Matrix ctm, ColorSpace colorspace, boolean alpha);
	public native StructuredText toStructuredText(String options);

	public StructuredText toStructuredText() {
		return toStructuredText(null);
	}

	public native Quad[][] search(String needle, int style);
	public Quad[][] search(String needle)
	{
		return search(needle, StructuredText.SEARCH_IGNORE_CASE);
	}

	public native void run(Device dev, Matrix ctm, Rect scissor, Cookie cookie);

	public void run(Device dev, Matrix ctm, Cookie cookie) {
		run(dev, ctm, null, cookie);
	}

	public native BarcodeInfo decodeBarcode(Rect subarea, float rotate);
	public BarcodeInfo decodeBarcode(Rect subarea) {
		return decodeBarcode(subarea, 0);
	}
	public BarcodeInfo decodeBarcode() {
		return decodeBarcode(Rect.Infinite(), 0);
	}
}
