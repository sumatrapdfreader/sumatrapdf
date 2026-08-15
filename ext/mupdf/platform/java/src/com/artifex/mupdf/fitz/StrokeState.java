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

public class StrokeState
{
	static {
		Context.init();
	}

	public static final int LINE_CAP_BUTT = 0;
	public static final int LINE_CAP_ROUND = 1;
	public static final int LINE_CAP_SQUARE = 2;
	public static final int LINE_CAP_TRIANGLE = 3;

	public static final int LINE_JOIN_MITER = 0;
	public static final int LINE_JOIN_ROUND = 1;
	public static final int LINE_JOIN_BEVEL = 2;
	public static final int LINE_JOIN_MITER_XPS = 3;

	private long pointer;

	protected native void finalize();

	public void destroy() {
		finalize();
	}

	private native long newNativeStrokeState(int lineCap, int lineJoin, float lineWidth, float miterLimit, float dashPhase, float[] dash);

	// Private constructor for the C to use. Any objects created by the
	// C are done for purposes of calling back to a java device, and
	// should therefore be considered const. This is fine as we don't
	// currently provide mechanisms for changing individual elements
	// of the StrokeState.
	private StrokeState(long p) {
		pointer = p;
	}

	public StrokeState(int lineCap, int lineJoin, float lineWidth, float miterLimit) {
		pointer = newNativeStrokeState(lineCap, lineJoin, lineWidth, miterLimit, 0, null);
	}

	public StrokeState(int lineCap, int lineJoin, float lineWidth, float miterLimit,
			float dashPhase, float[] dash) {
		pointer = newNativeStrokeState(lineCap, lineJoin, lineWidth, miterLimit, dashPhase, dash);
	}

	public native int getLineCap();
	public native int getLineJoin();
	public native float getLineWidth();
	public native float getMiterLimit();
	public native float getDashPhase();
	public native float[] getDashPattern();
}
