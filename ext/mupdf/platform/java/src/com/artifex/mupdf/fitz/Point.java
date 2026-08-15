// Copyright (C) 2004-2022 Artifex Software, Inc.
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

import java.util.Objects;

public class Point
{
	public float x;
	public float y;

	public Point(float x, float y) {
		this.x = x;
		this.y = y;
	}

	public Point(Point p) {
		this.x = p.x;
		this.y = p.y;
	}

	public String toString() {
		return "[" + x + " " + y + "]";
	}

	public Point transform(Matrix tm) {
		float old_x = this.x;

		this.x = old_x * tm.a + y * tm.c + tm.e;
		this.y = old_x * tm.b + y * tm.d + tm.f;

		return this;
	}

	@Override
	public boolean equals(Object obj) {
		if (!(obj instanceof Point))
			return false;
		Point other = (Point) obj;
		return x == other.x && y == other.y;
	}

	@Override
	public int hashCode() {
		return Objects.hash(x, y);
	}
}
