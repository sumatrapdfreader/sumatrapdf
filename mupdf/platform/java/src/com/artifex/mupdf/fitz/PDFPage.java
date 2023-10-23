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

public class PDFPage extends Page
{
	static {
		Context.init();
	}

	private PDFPage(long p) { super(p); }

	public native PDFObject getObject();
	public native PDFAnnotation[] getAnnotations();
	public native PDFAnnotation createAnnotation(int type);
	public native void deleteAnnotation(PDFAnnotation annot);

	public static final int REDACT_IMAGE_NONE = 0;
	public static final int REDACT_IMAGE_REMOVE = 1;
	public static final int REDACT_IMAGE_PIXELS = 2;

	public native boolean applyRedactions(boolean blackBoxes, int imageMethod);

	public boolean applyRedactions() {
		return applyRedactions(true, REDACT_IMAGE_PIXELS);
	}

	public native boolean update();

	public native PDFWidget[] getWidgets();

	public PDFWidget activateWidgetAt(float pageX, float pageY) {
		for (PDFWidget widget : getWidgets()) {
			if (widget.getBounds().contains(pageX, pageY)) {
				widget.eventEnter();
				widget.eventDown();
				widget.eventFocus();
				widget.eventUp();
				widget.eventExit();
				widget.eventBlur();
				return widget;
			}
		}
		return null;
	}

	public native PDFWidget createSignature();

	public native Matrix getTransform();

	public Link createLinkFit(Rect bbox, int page) {
		return createLink(bbox, LinkDestination.Fit(0, page));
	}
	public Link createLinkFitB(Rect bbox, int page) {
		return createLink(bbox, LinkDestination.FitB(0, page));
	}
	public Link createLinkXYZ(Rect bbox, int page, float x, float y, float zoom) {
		return createLink(bbox, LinkDestination.XYZ(0, page, x, y, zoom));
	}
	public Link createLinkFitR(Rect bbox, int page, float x, float y, float w, float h) {
		return createLink(bbox, LinkDestination.FitR(0, page, x, y, w, h));
	}
	public Link createLinkFitV(Rect bbox, int page, float x) {
		return createLink(bbox, LinkDestination.FitV(0, page, x));
	}
	public Link createLinkFitBV(Rect bbox, int page, float x) {
		return createLink(bbox, LinkDestination.FitBV(0, page, x));
	}
	public Link createLinkFitH(Rect bbox, int page, float y) {
		return createLink(bbox, LinkDestination.FitH(0, page, y));
	}
	public Link createLinkFitBH(Rect bbox, int page, float y) {
		return createLink(bbox, LinkDestination.FitBH(0, page, y));
	}

	// TODO: toPixmap with usage and page box
}
