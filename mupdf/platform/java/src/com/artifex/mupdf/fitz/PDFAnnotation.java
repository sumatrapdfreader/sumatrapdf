// Copyright (C) 2004-2023 Artifex Software, Inc.
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

import java.util.Date;
import java.util.Objects;

public class PDFAnnotation
{
	static {
		Context.init();
	}

	private long pointer;

	protected native void finalize();

	public void destroy() {
		finalize();
	}

	protected PDFAnnotation(long p) {
		pointer = p;
	}

	@Override
	public boolean equals(Object obj) {
		if (!(obj instanceof PDFAnnotation))
			return false;
		PDFAnnotation other = (PDFAnnotation) obj;
		return (this.pointer == other.pointer);
	}

	@Override
	public int hashCode() {
		return Objects.hash(pointer);
	}

	public native void run(Device dev, Matrix ctm, Cookie cookie);
	public native Pixmap toPixmap(Matrix ctm, ColorSpace colorspace, boolean alpha);
	public native Rect getBounds();
	public native DisplayList toDisplayList();

	/* IMPORTANT: Keep in sync with mupdf/pdf/annot.h */
	public static final int TYPE_TEXT = 0;
	public static final int TYPE_LINK = 1;
	public static final int TYPE_FREE_TEXT = 2;
	public static final int TYPE_LINE = 3;
	public static final int TYPE_SQUARE = 4;
	public static final int TYPE_CIRCLE = 5;
	public static final int TYPE_POLYGON = 6;
	public static final int TYPE_POLY_LINE = 7;
	public static final int TYPE_HIGHLIGHT = 8;
	public static final int TYPE_UNDERLINE = 9;
	public static final int TYPE_SQUIGGLY = 10;
	public static final int TYPE_STRIKE_OUT = 11;
	public static final int TYPE_REDACT = 12;
	public static final int TYPE_STAMP = 13;
	public static final int TYPE_CARET = 14;
	public static final int TYPE_INK = 15;
	public static final int TYPE_POPUP = 16;
	public static final int TYPE_FILE_ATTACHMENT = 17;
	public static final int TYPE_SOUND = 18;
	public static final int TYPE_MOVIE = 19;
	public static final int TYPE_RICH_MEDIA = 20;
	public static final int TYPE_WIDGET = 21;
	public static final int TYPE_SCREEN = 22;
	public static final int TYPE_PRINTER_MARK = 23;
	public static final int TYPE_TRAP_NET = 24;
	public static final int TYPE_WATERMARK = 25;
	public static final int TYPE_3D = 26;
	public static final int TYPE_PROJECTION = 27;
	public static final int TYPE_UNKNOWN = -1;

	public static final int LINE_ENDING_NONE = 0;
	public static final int LINE_ENDING_SQUARE = 1;
	public static final int LINE_ENDING_CIRCLE = 2;
	public static final int LINE_ENDING_DIAMOND = 3;
	public static final int LINE_ENDING_OPEN_ARROW = 4;
	public static final int LINE_ENDING_CLOSED_ARROW = 5;
	public static final int LINE_ENDING_BUTT = 6;
	public static final int LINE_ENDING_R_OPEN_ARROW = 7;
	public static final int LINE_ENDING_R_CLOSED_ARROW = 8;
	public static final int LINE_ENDING_SLASH = 9;

	public static final int BORDER_STYLE_SOLID = 0;
	public static final int BORDER_STYLE_DASHED = 1;
	public static final int BORDER_STYLE_BEVELED = 2;
	public static final int BORDER_STYLE_INSET = 3;
	public static final int BORDER_STYLE_UNDERLINE = 4;

	public static final int BORDER_EFFECT_NONE = 0;
	public static final int BORDER_EFFECT_CLOUDY = 1;

	public static final int IS_INVISIBLE = 1 << (1-1);
	public static final int IS_HIDDEN = 1 << (2-1);
	public static final int IS_PRINT = 1 << (3-1);
	public static final int IS_NO_ZOOM = 1 << (4-1);
	public static final int IS_NO_ROTATE = 1 << (5-1);
	public static final int IS_NO_VIEW = 1 << (6-1);
	public static final int IS_READ_ONLY = 1 << (7-1);
	public static final int IS_LOCKED = 1 << (8-1);
	public static final int IS_TOGGLE_NO_VIEW = 1 << (9-1);
	public static final int IS_LOCKED_CONTENTS = 1 << (10-1);

	public native int getType();
	public native int getFlags();
	public native void setFlags(int flags);
	public native String getContents();
	public native void setContents(String contents);
	public native float getBorder();
	public native void setBorder(float width);
	public native float[] getColor();
	public native void setColor(float[] color);
	public native float getOpacity();
	public native void setOpacity(float opacity);

	protected native long getCreationDateNative();
	protected native void setCreationDate(long time);
	protected native long getModificationDateNative();
	protected native void setModificationDate(long time);
	public Date getCreationDate() {
		return new Date(getCreationDateNative());
	}
	public void setCreationDate(Date date) {
		setCreationDate(date.getTime());
	}
	public Date getModificationDate() {
		return new Date(getModificationDateNative());
	}
	public void setModificationDate(Date date) {
		setModificationDate(date.getTime());
	}

	public native boolean hasRect();
	public native Rect getRect();
	public native void setRect(Rect rect);

	public native boolean hasInteriorColor();
	public native float[] getInteriorColor();
	public native void setInteriorColor(float[] color);

	public native boolean hasAuthor();
	public native String getAuthor();
	public native void setAuthor(String author);

	public native boolean hasLineEndingStyles();
	public native int[] getLineEndingStyles();
	public native void setLineEndingStyles(int startStyle, int endStyle);
	public void setLineEndingStyles(int[] styles) {
		setLineEndingStyles(styles[0], styles[1]);
	}

	public native boolean hasBorder();
	public native int getBorderStyle();
	public native void setBorderStyle(int style);
	public native float getBorderWidth();
	public native void setBorderWidth(float width);
	public native int getBorderDashCount();
	public native float getBorderDashItem(int i);
	public native void clearBorderDash();
	public native void addBorderDashItem(float length);
	public void setBorderDashPattern(float[] dash_pattern)
	{
		clearBorderDash();
		for (float length : dash_pattern)
			addBorderDashItem(length);
	}

	public native boolean hasBorderEffect();
	public native int getBorderEffect();
	public native void setBorderEffect(int effect);
	public native float getBorderEffectIntensity();
	public native void setBorderEffectIntensity(float intensity);

	public native boolean hasQuadPoints();
	public native int getQuadPointCount();
	public native Quad getQuadPoint(int i);
	public native void clearQuadPoints();
	public native void addQuadPoint(Quad q);
	public Quad[] getQuadPoints() {
		int n = getQuadPointCount();
		Quad[] list = new Quad[n];
		for (int i = 0; i < n; ++i)
			list[i] = getQuadPoint(i);
		return list;
	}
	public void setQuadPoints(Quad[] quadPoints) {
		clearQuadPoints();
		for (Quad q : quadPoints)
			addQuadPoint(q);
	}

	public native boolean hasVertices();
	public native int getVertexCount();
	public native Point getVertex(int i);
	public native void clearVertices();
	public native void addVertex(float x, float y);
	public void addVertex(Point p) {
		addVertex(p.x, p.y);
	}
	public Point[] getVertices() {
		int n = getVertexCount();
		Point[] list = new Point[n];
		for (int i = 0; i < n; ++i)
			list[i] = getVertex(i);
		return list;
	}
	public void setVertices(Point[] vertices) {
		clearVertices();
		for (Point p : vertices)
			addVertex(p);
	}

	public native boolean hasInkList();
	public native int getInkListCount();
	public native int getInkListStrokeCount(int i);
	public native Point getInkListStrokeVertex(int i, int k);
	public native void clearInkList();
	public native void addInkListStroke();
	public native void addInkListStrokeVertex(float x, float y);
	public void addInkListStrokeVertex(Point p) {
		addInkListStrokeVertex(p.x, p.y);
	}
	public void addInkList(Point[] stroke) {
		addInkListStroke();
		for (Point p : stroke)
			addInkListStrokeVertex(p);
	}
	public void setInkList(Point[][] inkList) {
		clearInkList();
		for (Point[] stroke : inkList) {
			addInkListStroke();
			for (Point p : stroke)
				addInkListStrokeVertex(p);
		}
	}
	public Point[][] getInkList() {
		int i, k, n, m = getInkListCount();
		Point[][] list = new Point[m][];
		for (i = 0; i < m; ++i) {
			n = getInkListStrokeCount(i);
			list[i] = new Point[n];
			for (k = 0; k < n; ++k)
				list[i][k] = getInkListStrokeVertex(i, k);
		}
		return list;
	}

	public native boolean hasIcon();
	public native String getIcon();
	public native void setIcon(String icon);

	public native boolean hasOpen();
	public native boolean getIsOpen();
	public native void setIsOpen(boolean open);

	public native boolean hasLine();
	public native Point[] getLine();
	public native void setLine(Point a, Point b);

	public native boolean hasFileSpecification();
	public native void setFileSpecification(PDFObject fs);
	public native PDFObject getFileSpecification();

	public native void eventEnter();
	public native void eventExit();
	public native void eventDown();
	public native void eventUp();
	public native void eventFocus();
	public native void eventBlur();

	public native boolean update();

	public native PDFObject getObject();

	/* See PDFDocument.LANGUAGE_* */
	public native int getLanguage();
	public native void setLanguage(int lang);

	public native int getQuadding();
	public native void setQuadding(int quadding);

	public native DefaultAppearance getDefaultAppearance();
	public native void setDefaultAppearance(String font, float size, float[] color);

	protected native void setNativeAppearance(String appearance, String state, Matrix ctm, Rect bbox, PDFObject res, Buffer contents);
	protected native void setNativeAppearanceDisplayList(String appearance, String state, Matrix ctm, DisplayList list);
	protected native void setNativeAppearanceImage(Image image);

	public void setAppearance(String appearance, String state, Matrix ctm, Rect bbox, PDFObject res, Buffer contents) {
		setNativeAppearance(appearance, state, ctm, bbox, res, contents);
	}
	public void setAppearance(String appearance, Matrix ctm, Rect bbox, PDFObject res, Buffer contents) {
		setNativeAppearance(appearance, null, ctm, bbox, res, contents);
	}
	public void setAppearance(String appearance, Rect bbox, PDFObject res, Buffer contents) {
		setNativeAppearance(appearance, null, null, bbox, res, contents);
	}
	public void setAppearance(Matrix ctm, Rect bbox, PDFObject res, Buffer contents) {
		setNativeAppearance(null, null, ctm, bbox, res, contents);
	}
	public void setAppearance(Rect bbox, PDFObject res, Buffer contents) {
		setNativeAppearance(null, null, null, bbox, res, contents);
	}
	public void setAppearance(String appearance, String state, Matrix ctm, DisplayList list) {
		setNativeAppearanceDisplayList(appearance, state, ctm, list);
	}
	public void setAppearance(String appearance, Matrix ctm, DisplayList list) {
		setNativeAppearanceDisplayList(appearance, null, ctm, list);
	}
	public void setAppearance(String appearance, DisplayList list) {
		setNativeAppearanceDisplayList(appearance, null, null, list);
	}
	public void setAppearance(Matrix ctm, DisplayList list) {
		setNativeAppearanceDisplayList(null, null, ctm, list);
	}
	public void setAppearance(DisplayList list) {
		setNativeAppearanceDisplayList(null, null, null, list);
	}
	public void setAppearance(Image image) {
		setNativeAppearanceImage(image);
	}

	public native boolean getHiddenForEditing();
	public native void setHiddenForEditing(boolean hidden);

	public native boolean applyRedaction(boolean blackBoxes, int imageMethod);
}
