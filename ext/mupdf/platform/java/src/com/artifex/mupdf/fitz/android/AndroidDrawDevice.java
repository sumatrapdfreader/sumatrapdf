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

package com.artifex.mupdf.fitz.android;

import android.graphics.Bitmap;

import com.artifex.mupdf.fitz.Context;
import com.artifex.mupdf.fitz.Matrix;
import com.artifex.mupdf.fitz.NativeDevice;
import com.artifex.mupdf.fitz.Page;
import com.artifex.mupdf.fitz.Rect;
import com.artifex.mupdf.fitz.RectI;

public final class AndroidDrawDevice extends NativeDevice
{
	static {
		Context.init();
	}

	private native long newNative(Bitmap bitmap, int xOrigin, int yOrigin, int patchX0, int patchY0, int patchX1, int patchY1, boolean clear);

	public AndroidDrawDevice(Bitmap bitmap, int xOrigin, int yOrigin, int patchX0, int patchY0, int patchX1, int patchY1, boolean clear) {
		super(0);
		pointer = newNative(bitmap, xOrigin, yOrigin, patchX0, patchY0, patchX1, patchY1, clear);
	}

	public AndroidDrawDevice(Bitmap bitmap, int xOrigin, int yOrigin, int patchX0, int patchY0, int patchX1, int patchY1) {
		this(bitmap, xOrigin, yOrigin, patchX0, patchY0, patchX1, patchY1, true);
	}

	public AndroidDrawDevice(Bitmap bitmap, int xOrigin, int yOrigin, boolean clear) {
		this(bitmap, xOrigin, yOrigin, 0, 0, bitmap.getWidth(), bitmap.getHeight(), clear);
	}

	public AndroidDrawDevice(Bitmap bitmap, int xOrigin, int yOrigin) {
		this(bitmap, xOrigin, yOrigin, 0, 0, bitmap.getWidth(), bitmap.getHeight(), true);
	}

	public AndroidDrawDevice(Bitmap bitmap, boolean clear) {
		this(bitmap, 0, 0, clear);
	}

	public AndroidDrawDevice(Bitmap bitmap) {
		this(bitmap, 0, 0, true);
	}

	public static Bitmap drawPage(Page page, Matrix ctm) {
		RectI ibox = new RectI(page.getBounds().transform(ctm));
		int w = ibox.x1 - ibox.x0;
		int h = ibox.y1 - ibox.y0;
		Bitmap bmp = Bitmap.createBitmap(w, h, Bitmap.Config.ARGB_8888);
		AndroidDrawDevice dev = new AndroidDrawDevice(bmp, ibox.x0, ibox.y0);
		try {
			page.run(dev, ctm, null);
			dev.close();
		} finally {
			dev.destroy();
		}
		return bmp;
	}

	public static Bitmap drawPage(Page page, float dpi, int rotate) {
		return drawPage(page, new Matrix(dpi / 72).rotate(rotate));
	}

	public static Bitmap drawPage(Page page, float dpi) {
		return drawPage(page, new Matrix(dpi / 72));
	}

	public static Matrix fitPage(Page page, int fitW, int fitH) {
		Rect bbox = page.getBounds();
		float pageW = bbox.x1 - bbox.x0;
		float pageH = bbox.y1 - bbox.y0;
		float scaleH = (float)fitW / pageW;
		float scaleV = (float)fitH / pageH;
		float scale = scaleH < scaleV ? scaleH : scaleV;
		scaleH = (float)Math.floor(pageW * scale) / pageW;
		scaleV = (float)Math.floor(pageH * scale) / pageH;
		return new Matrix(scaleH, scaleV);
	}

	public static Bitmap drawPageFit(Page page, int fitW, int fitH) {
		return drawPage(page, fitPage(page, fitW, fitH));
	}

	public static Matrix fitPageWidth(Page page, int fitW) {
		Rect bbox = page.getBounds();
		float pageW = bbox.x1 - bbox.x0;
		float scale = (float)fitW / pageW;
		scale = (float)Math.floor(pageW * scale) / pageW;
		return new Matrix(scale);
	}

	public static Bitmap drawPageFitWidth(Page page, int fitW) {
		return drawPage(page, fitPageWidth(page, fitW));
	}

	public native final void invertLuminance();
}
