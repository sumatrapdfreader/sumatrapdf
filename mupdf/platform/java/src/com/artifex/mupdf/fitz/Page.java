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

public class Page
{
	static {
		Context.init();
	}

	public static final int MEDIA_BOX = 0;
	public static final int CROP_BOX = 1;
	public static final int BLEED_BOX = 2;
	public static final int TRIM_BOX = 3;
	public static final int ART_BOX = 4;
	public static final int UNKNOWN_BOX = 5;

	private long pointer;

	protected native void finalize();

	public void destroy() {
		finalize();
	}

	protected Page(long p) {
		pointer = p;
	}

	private native Rect getBoundsNative(int box);

	public Rect getBounds(int box) {
		return getBoundsNative(box);
	}

	public Rect getBounds() {
		return getBoundsNative(Page.CROP_BOX);
	}

	public native void run(Device dev, Matrix ctm, Cookie cookie);
	public native void runPageContents(Device dev, Matrix ctm, Cookie cookie);
	public native void runPageAnnots(Device dev, Matrix ctm, Cookie cookie);
	public native void runPageWidgets(Device dev, Matrix ctm, Cookie cookie);

	public void run(Device dev, Matrix ctm) {
		run(dev, ctm, null);
	}

	public native Link[] getLinks();

	public native Pixmap toPixmap(Matrix ctm, ColorSpace cs, boolean alpha, boolean showExtras);
	public Pixmap toPixmap(Matrix ctm, ColorSpace cs, boolean alpha) {
		return toPixmap(ctm, cs, alpha, true);
	}

	public native DisplayList toDisplayList(boolean showExtras);
	public DisplayList toDisplayList() {
		return toDisplayList(true);
	}

	public native StructuredText toStructuredText(String options);
	public StructuredText toStructuredText() {
		return toStructuredText(null);
	}

	public native Quad[][] search(String needle, int style);
	public Quad[][] search(String needle)
	{
		return search(needle, StructuredText.SEARCH_IGNORE_CASE);
	}

	public native byte[] textAsHtml();

	public native Document getDocument();

	public native Link createLink(Rect bbox, String uri);
	public Link createLink(Rect bbox, LinkDestination dest) {
		return createLink(bbox, getDocument().formatLinkURI(dest));
	}
	public native void deleteLink(Link link);

	public native String getLabel();

	public boolean isPDF() {
		return false;
	}

	public native BarcodeInfo decodeBarcode(Rect subarea, float rotate);
	public BarcodeInfo decodeBarcode(Rect subarea) {
		return decodeBarcode(subarea, 0);
	}
	public BarcodeInfo decodeBarcode() {
		return decodeBarcode(Rect.Infinite(), 0);
	}
}
