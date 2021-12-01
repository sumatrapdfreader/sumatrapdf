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

public class PDFPage extends Page
{
	static {
		Context.init();
	}

	private PDFPage(long p) { super(p); }

	public native PDFAnnotation[] getAnnotations();
	public native PDFAnnotation createAnnotation(int subtype);
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

	/* type is one of PDFDocument.DESTINATION_* */
	public native Link createLink(Rect bbox, int pageno, int type, float ... args);

	public Link createLinkXYZ(Rect bbox, int pageno, float left, float top, float zoom)
	{
		return createLink(bbox, pageno, PDFDocument.DESTINATION_XYZ, left, top, zoom);
	}
	public Link createLinkXYZ(Rect bbox, int pageno, Point p, float zoom)
	{
		return createLinkXYZ(bbox, pageno, p.x, p.y, zoom);
	}

	public Link createLinkFit(Rect bbox, int pageno)
	{
		return createLink(bbox, pageno, PDFDocument.DESTINATION_FIT);
	}

	public Link createLinkFitH(Rect bbox, int pageno, float top)
	{
		return createLink(bbox, pageno, PDFDocument.DESTINATION_FIT_H, top);
	}

	public Link createLinkFitV(Rect bbox, int pageno, float left)
	{
		return createLink(bbox, pageno, PDFDocument.DESTINATION_FIT_V, left);
	}

	public Link createLinkFitR(Rect bbox, int pageno, float left, float bottom, float right, float top)
	{
		return createLink(bbox, pageno, PDFDocument.DESTINATION_FIT_R, left, bottom, right, top);
	}
	public Link createLinkFitR(Rect bbox, int pageno, Rect area)
	{
		return createLinkFitR(bbox, pageno, area.x0, area.y0, area.x1, area.y0);
	}

	public Link createLinkFitB(Rect bbox, int pageno)
	{
		return createLink(bbox, pageno, PDFDocument.DESTINATION_FIT_B);
	}

	public Link createLinkFitBH(Rect bbox, int pageno, float top)
	{
		return createLink(bbox, pageno, PDFDocument.DESTINATION_FIT_BH, top);
	}

	public Link createLinkFitBV(Rect bbox, int pageno, float left)
	{
		return createLink(bbox, pageno, PDFDocument.DESTINATION_FIT_BV, left);
	}
}
