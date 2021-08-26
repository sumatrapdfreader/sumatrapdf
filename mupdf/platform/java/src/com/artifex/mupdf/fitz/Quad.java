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

public class Quad
{
	public float ul_x, ul_y;
	public float ur_x, ur_y;
	public float ll_x, ll_y;
	public float lr_x, lr_y;

	public Quad(float ul_x, float ul_y, float ur_x, float ur_y, float ll_x, float ll_y, float lr_x, float lr_y) {
		this.ul_x = ul_x;
		this.ul_y = ul_y;
		this.ur_x = ur_x;
		this.ur_y = ur_y;
		this.ll_x = ll_x;
		this.ll_y = ll_y;
		this.lr_x = lr_x;
		this.lr_y = lr_y;
	}

	public Rect toRect() {
		float x0 = Math.min(Math.min(ul_x, ur_x), Math.min(ll_x, lr_x));
		float y0 = Math.min(Math.min(ul_y, ur_y), Math.min(ll_y, lr_y));
		float x1 = Math.max(Math.max(ul_x, ur_x), Math.max(ll_x, lr_x));
		float y1 = Math.max(Math.max(ul_y, ur_y), Math.max(ll_y, lr_y));
		return new Rect(x0, y0, x1, y1);
	}

	public Quad transformed(Matrix m) {
		float t_ul_x = ul_x * m.a + ul_y * m.c + m.e;
		float t_ul_y = ul_x * m.b + ul_y * m.d + m.f;
		float t_ur_x = ur_x * m.a + ur_y * m.c + m.e;
		float t_ur_y = ur_x * m.b + ur_y * m.d + m.f;
		float t_ll_x = ll_x * m.a + ll_y * m.c + m.e;
		float t_ll_y = ll_x * m.b + ll_y * m.d + m.f;
		float t_lr_x = lr_x * m.a + lr_y * m.c + m.e;
		float t_lr_y = lr_x * m.b + lr_y * m.d + m.f;
		return new Quad(
			t_ul_x, t_ul_y,
			t_ur_x, t_ur_y,
			t_ll_x, t_ll_y,
			t_lr_x, t_lr_y
		);
	}

	public Quad transform(Matrix m) {
		float t_ul_x = ul_x * m.a + ul_y * m.c + m.e;
		float t_ul_y = ul_x * m.b + ul_y * m.d + m.f;
		float t_ur_x = ur_x * m.a + ur_y * m.c + m.e;
		float t_ur_y = ur_x * m.b + ur_y * m.d + m.f;
		float t_ll_x = ll_x * m.a + ll_y * m.c + m.e;
		float t_ll_y = ll_x * m.b + ll_y * m.d + m.f;
		float t_lr_x = lr_x * m.a + lr_y * m.c + m.e;
		float t_lr_y = lr_x * m.b + lr_y * m.d + m.f;
		ul_x = t_ul_x;
		ul_y = t_ul_y;
		ur_x = t_ur_x;
		ur_y = t_ur_y;
		ll_x = t_ll_x;
		ll_y = t_ll_y;
		lr_x = t_lr_x;
		lr_y = t_lr_y;
		return this;
	}

	public String toString() {
		return "["
			+ ul_x + " " + ul_y + " "
			+ ur_x + " " + ur_y + " "
			+ ll_x + " " + ll_y + " "
			+ lr_x + " " + lr_y
			+ "]";
	}
}
