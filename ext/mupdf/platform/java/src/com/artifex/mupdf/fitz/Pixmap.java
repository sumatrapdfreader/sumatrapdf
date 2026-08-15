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

public class Pixmap
{
	static {
		Context.init();
	}

	private long pointer;

	protected native void finalize();

	public void destroy() {
		finalize();
	}

	private native long newNative(ColorSpace cs, int x, int y, int w, int h, boolean alpha);
	private native long newNativeFromColorAndMask(Pixmap color, Pixmap mask);
	private native long newNativeDeskew(float angle, int border);

	private Pixmap(long p) {
		pointer = p;
	}

	public Pixmap(ColorSpace cs, int x, int y, int w, int h, boolean alpha) {
		pointer = newNative(cs, x, y, w, h, alpha);
	}

	public Pixmap(ColorSpace cs, int x, int y, int w, int h) {
		this(cs, x, y, w, h, false);
	}

	public Pixmap(ColorSpace cs, int w, int h, boolean alpha) {
		this(cs, 0, 0, w, h, alpha);
	}

	public Pixmap(ColorSpace cs, int w, int h) {
		this(cs, 0, 0, w, h, false);
	}

	public Pixmap(ColorSpace cs, Rect rect, boolean alpha) {
		this(cs, (int)rect.x0, (int)rect.y0, (int)(rect.x1 - rect.x0), (int)(rect.y1 - rect.y0), alpha);
	}

	public Pixmap(ColorSpace cs, Rect rect) {
		this(cs, rect, false);
	}

	public Pixmap(Pixmap color, Pixmap mask) {
		pointer = newNativeFromColorAndMask(color, mask);
	}

	public native void clear();
	private native void clearWithValue(int value);
	public void clear(int value) {
		clearWithValue(value);
	}

	public native Buffer asPNG();
	public native Buffer asJPEG(int quality, boolean invertCMYK);
	public native Buffer asPAM();
	public native Buffer asPNM();
	public native Buffer asPBM();
	public native Buffer asPKM();
	public native Buffer asJPX(int quality);

	public native void saveAsPNG(String filename);
	public native void saveAsJPEG(String filename, int quality);
	public native void saveAsPAM(String filename);
	public native void saveAsPNM(String filename);
	public native void saveAsPBM(String filename);
	public native void saveAsPKM(String filename);
	public native void saveAsJPX(String filename, int quality);

	public native int getX();
	public native int getY();
	public native int getWidth();
	public native int getHeight();
	public native int getStride();
	public native int getNumberOfComponents();
	public native boolean getAlpha();
	public native ColorSpace getColorSpace();
	public native byte[] getSamples();
	public native byte getSample(int x, int y, int n);
	public native int[] getPixels(); /* only valid for RGBA or BGRA pixmaps */
	public native int getXResolution();
	public native int getYResolution();

	public native void setResolution(int xres, int yres);

	public native void invert();
	public native void invertLuminance();
	public native void gamma(float gamma);
	public native void tint(int black, int white);
	public native Pixmap convertToColorSpace(ColorSpace cs, ColorSpace proof, DefaultColorSpaces defaultCs, int colorParams, boolean keepAlpha);
	public native byte[] computeMD5();

	public Rect getBounds() {
		int x = getX();
		int y = getY();
		return new Rect(x, y, x + getWidth(), y+ getHeight());
	}

	public String toString() {
		return "Pixmap(w=" + getWidth() +
			" h=" + getHeight() +
			" x=" + getX() +
			" y=" + getY() +
			" n=" + getNumberOfComponents() +
			" alpha=" + getAlpha() +
			" cs=" + getColorSpace() +
			")";
	}

	public static final int DESKEW_BORDER_INCREASE = 0;
	public static final int DESKEW_BORDER_MAINTAIN = 1;
	public static final int DESKEW_BORDER_DECREASE = 2;

	public Pixmap deskew(float angle, int border)
	{
		return new Pixmap(newNativeDeskew(angle, border));
	}

	public native float detectSkew();
	public native Pixmap warp(Quad points, int width, int height);
	public native Pixmap autowarp(Quad points);

	private native Quad detectDocument();

	public native BarcodeInfo decodeBarcode(float rotate);
	public BarcodeInfo decodeBarcode() {
		return decodeBarcode(0);
	}

	public static native Pixmap encodeBarcode(int type, String contents, int size, int errorCorrectionLevel, boolean quietZones, boolean humanReadableText);
	public static Pixmap encodeBarcode(int type, String contents, int size, int errorCorrectionLevel, boolean quietZones) {
		return encodeBarcode(type, contents, size, errorCorrectionLevel, quietZones, false);
	}
	public static Pixmap encodeBarcode(int type, String contents, int size, int errorCorrectionLevel) {
		return encodeBarcode(type, contents, size, errorCorrectionLevel, false, false);
	}
	public static Pixmap encodeBarcode(int type, String contents, int size) {
		return encodeBarcode(type, contents, size, 2, false, false);
	}
	public static Pixmap encodeBarcode(int type, String contents) {
		return encodeBarcode(type, contents, 0, 2, false, false);
	}
}
