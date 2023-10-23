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

public class Matrix
{
	public float a, b, c, d, e, f;

	public Matrix(float a, float b, float c, float d, float e, float f) {
		this.a = a;
		this.b = b;
		this.c = c;
		this.d = d;
		this.e = e;
		this.f = f;
	}

	public Matrix(float a, float b, float c, float d) {
		this(a, b, c, d, 0, 0);
	}

	public Matrix(float a, float d) {
		this(a, 0, 0, d, 0, 0);
	}

	public Matrix(float a) {
		this(a, 0, 0, a, 0, 0);
	}

	public Matrix() {
		this(1, 0, 0, 1, 0, 0);
	}

	public Matrix(Matrix copy) {
		this(copy.a, copy.b, copy.c, copy.d, copy.e, copy.f);
	}

	public Matrix(Matrix one, Matrix two) {
		a = one.a * two.a + one.b * two.c;
		b = one.a * two.b + one.b * two.d;
		c = one.c * two.a + one.d * two.c;
		d = one.c * two.b + one.d * two.d;
		e = one.e * two.a + one.f * two.c + two.e;
		f = one.e * two.b + one.f * two.d + two.f;
	}

	public Matrix concat(Matrix m) {
		float a = this.a * m.a + this.b * m.c;
		float b = this.a * m.b + this.b * m.d;
		float c = this.c * m.a + this.d * m.c;
		float d = this.c * m.b + this.d * m.d;
		float e = this.e * m.a + this.f * m.c + m.e;
		this.f = this.e * m.b + this.f * m.d + m.f;

		this.a = a;
		this.b = b;
		this.c = c;
		this.d = d;
		this.e = e;

		return this;
	}

	public Matrix scale(float sx, float sy) {
		a *= sx;
		b *= sx;
		c *= sy;
		d *= sy;
		return this;
	}

	public Matrix scale(float s) {
		return scale(s, s);
	}

	public Matrix translate(float tx, float ty) {
		e += tx * a + ty * c;
		f += tx * b + ty * d;
		return this;
	}

	public Matrix invert() {
		float det = a * d - b * c;
		if (det > -Math.ulp(0) && det < Math.ulp(0))
			return this;

		float srca = a;
		float srcb = b;
		float srcc = c;
		float srcd = d;
		float srce = e;
		float srcf = f;
		float rdet = 1 / det;
		this.a = srcd * rdet;
		this.b = -srcb * rdet;
		this.c = -srcc * rdet;
		this.d = srca * rdet;
		this.e = -srce * this.a - srcf * this.c;
		this.f = -srce * this.b - srcf * this.d;
		return this;
	}

	public Matrix rotate(float degrees) {
		while (degrees < 0)
			degrees += 360;
		while (degrees >= 360)
			degrees -= 360;

		if (Math.abs(0 - degrees) < 0.0001) {
			// Nothing to do
		} else if (Math.abs(90 - degrees) < 0.0001) {
			float save_a = a;
			float save_b = b;
			a = c;
			b = d;
			c = -save_a;
			d = -save_b;
		} else if (Math.abs(180 - degrees) < 0.0001) {
			a = -a;
			b = -b;
			c = -c;
			d = -d;
		} else if (Math.abs(270 - degrees) < 0.0001) {
			float save_a = a;
			float save_b = b;
			a = -c;
			b = -d;
			c = save_a;
			d = save_b;
		} else {
			float sin = (float)Math.sin(degrees * Math.PI / 180.0);
			float cos = (float)Math.cos(degrees * Math.PI / 180.0);
			float save_a = a;
			float save_b = b;
			a = cos * save_a + sin * c;
			b = cos * save_b + sin * d;
			c = -sin * save_a + cos * c;
			d = -sin * save_b + cos * d;
		}
		return this;
	}

	public String toString() {
		return "[" + a + " " + b + " " + c + " " + d + " " + e + " " + f + "]";
	}

	public static Matrix Identity() {
		return new Matrix(1, 0, 0, 1, 0, 0);
	}

	public static Matrix Scale(float x) {
		return new Matrix(x, 0, 0, x, 0, 0);
	}

	public static Matrix Scale(float x, float y) {
		return new Matrix(x, 0, 0, y, 0, 0);
	}

	public static Matrix Translate(float x, float y) {
		return new Matrix(1, 0, 0, 1, x, y);
	}

	public static Matrix Rotate(float degrees) {
		float sin, cos;

		while (degrees < 0)
			degrees += 360;
		while (degrees >= 360)
			degrees -= 360;

		if (Math.abs(0 - degrees) < 0.0001) {
			sin = 0;
			cos = 1;
		} else if (Math.abs(90 - degrees) < 0.0001) {
			sin = 1;
			cos = 0;
		} else if (Math.abs(180 - degrees) < 0.0001) {
			sin = 0;
			cos = -1;
		} else if (Math.abs(270 - degrees) < 0.0001) {
			sin = -1;
			cos = 0;
		} else {
			sin = (float)Math.sin(degrees * Math.PI / 180.0);
			cos = (float)Math.cos(degrees * Math.PI / 180.0);
		}

		return new Matrix(cos, sin, -sin, cos, 0, 0);
	}

	public static Matrix Inverted(Matrix m) {
		float det = m.a * m.d - m.b * m.c;
		if (det > -Math.ulp(0) && det < Math.ulp(0))
			return m;

		float rdet = 1 / det;
		float inva = m.d * rdet;
		float invb = -m.b * rdet;
		float invc = -m.c * rdet;
		float invd = m.a * rdet;
		float inve = -m.e * inva - m.f * invc;
		float invf = -m.e * invb - m.f * invd;

		return new Matrix(inva, invb, invc, invd, inve, invf);
	}
}
