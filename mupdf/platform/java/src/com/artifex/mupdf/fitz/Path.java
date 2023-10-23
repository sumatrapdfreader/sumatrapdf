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

public class Path implements PathWalker
{
	static {
		Context.init();
	}

	private long pointer;

	protected native void finalize();

	public void destroy() {
		finalize();
	}

	private native long newNative();
	private native long cloneNative();

	public Path() {
		pointer = newNative();
	}

	private Path(long p) {
		pointer = p;
	}

	public Path(Path old) {
		pointer = old.cloneNative();
	}

	public native Point currentPoint();

	public native void moveTo(float x, float y);
	public native void lineTo(float x, float y);
	public native void curveTo(float cx1, float cy1, float cx2, float cy2, float ex, float ey);
	public native void curveToV(float cx, float cy, float ex, float ey);
	public native void curveToY(float cx, float cy, float ex, float ey);
	public native void rect(int x1, int y1, int x2, int y2);
	public native void closePath();

	public void moveTo(Point xy) {
		moveTo(xy.x, xy.y);
	}

	public void lineTo(Point xy) {
		lineTo(xy.x, xy.y);
	}

	public void curveTo(Point c1, Point c2, Point e) {
		curveTo(c1.x, c1.y, c2.x, c2.y, e.x, e.y);
	}

	public void curveToV(Point c, Point e) {
		curveToV(c.x, c.y, e.x, e.y);
	}

	public void curveToY(Point c, Point e) {
		curveToY(c.x, c.y, e.x, e.y);
	}

	public native void transform(Matrix mat);

	public native Rect getBounds(StrokeState stroke, Matrix ctm);

	public native void walk(PathWalker walker);

	public String toString(String separator) {
		StringBuffer s = new StringBuffer();
		this.walk(new ToStringWalker(s, separator));
		return s.toString();
	}

	public String toString() {
		return toString(" ");
	}

	class ToStringWalker implements PathWalker {
		StringBuffer out;
		String separator;

		public ToStringWalker(StringBuffer out, String separator) {
			this.out = out;
			this.separator = separator;
		}

		public void moveTo(float x, float y) {
			out.append(
				Float.toString(x) + " " + Float.toString(y) +
				" m" + separator);
		}

		public void lineTo(float x, float y) {
			out.append(
				Float.toString(x) + " " + Float.toString(y) +
				" l" + separator);
		}

		public void curveTo(float cx1, float cy1, float cx2, float cy2, float ex, float ey) {
			out.append(
				Float.toString(cx1) + " " + Float.toString(cy1) +
				Float.toString(cx2) + " " + Float.toString(cy2) +
				" c" + separator);
		}

		public void closePath() {
			out.append("h" + separator);
		}
	}
}
