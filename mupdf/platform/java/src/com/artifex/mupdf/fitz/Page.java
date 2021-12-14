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

public class Page
{
	static {
		Context.init();
	}

	private long pointer;

	protected native void finalize();

	public void destroy() {
		finalize();
	}

	protected Page(long p) {
		pointer = p;
	}

	public native Rect getBounds();

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

	public native Quad[][] search(String needle);

	public native byte[] textAsHtml();

	public native Document getDocument();

	public native Link createLink(Rect bbox, String uri);
	public Link createLink(Rect bbox, LinkDestination dest) {
		return createLink(bbox, getDocument().formatLinkURI(dest));
	}
}
