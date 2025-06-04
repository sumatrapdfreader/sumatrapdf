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

abstract public class PDFProcessor
{
	static {
		Context.init();
	}

	abstract public void pushResources(PDFObject resources);
	abstract public void popResources();
	abstract public void op_gs(String name, PDFObject extgstate);

	abstract public void op_w(float lineWidth);
	abstract public void op_j(float lineJoin);
	abstract public void op_J(float lineCap);
	abstract public void op_M(float miterLimit);
	abstract public void op_d(float[] array, float phase);
	abstract public void op_ri(String intent);
	abstract public void op_i(float flatness);

	abstract public void op_q();
	abstract public void op_Q();
	abstract public void op_cm(float a, float b, float c, float d, float e, float f);

	abstract public void op_m(float x, float y);
	abstract public void op_l(float x, float y);
	abstract public void op_c(float x1, float y1, float x2, float y2, float x3, float y3);
	abstract public void op_v(float x2, float y2, float x3, float y3);
	abstract public void op_y(float x1, float y1, float x3, float y3);
	abstract public void op_h();
	abstract public void op_re(float x, float y, float w, float h);

	abstract public void op_S();
	abstract public void op_s();
	abstract public void op_F();
	abstract public void op_f();
	abstract public void op_fstar();
	abstract public void op_B();
	abstract public void op_Bstar();
	abstract public void op_b();
	abstract public void op_bstar();
	abstract public void op_n();

	abstract public void op_W();
	abstract public void op_Wstar();

	abstract public void op_BT();
	abstract public void op_ET();

	abstract public void op_Tc(float charspace);
	abstract public void op_Tw(float wordspace);
	abstract public void op_Tz(float scale);
	abstract public void op_TL(float leading);
	abstract public void op_Tf(String name, float size);
	abstract public void op_Tr(float render);
	abstract public void op_Ts(float rise);

	abstract public void op_Td(float tx, float ty);
	abstract public void op_TD(float tx, float ty);
	abstract public void op_Tm(float a, float b, float c, float d, float e, float f);
	abstract public void op_Tstar();

	abstract public void op_TJ(Object[] array);
	abstract public void op_Tj(String string);
	abstract public void op_Tj(byte[] array);
	abstract public void op_squote(String text);
	abstract public void op_squote(byte[] text);
	abstract public void op_dquote(float aw, float ac, String text);
	abstract public void op_dquote(float aw, float ac, byte[] text);

	abstract public void op_d0(float wx, float wy);
	abstract public void op_d1(float wx, float wy, float llx, float lly, float urx, float ury);

	abstract public void op_CS(String name, ColorSpace cs);
	abstract public void op_cs(String name, ColorSpace cs);
	abstract public void op_SC_color(float[] color);
	abstract public void op_sc_color(float[] color);
	abstract public void op_SC_pattern(String name, int idx, float[] color);
	abstract public void op_sc_pattern(String name, int idx, float[] color);
	abstract public void op_SC_shade(String name);
	abstract public void op_sc_shade(String name);

	abstract public void op_G(float g);
	abstract public void op_g(float g);
	abstract public void op_RG(float r, float g, float b);
	abstract public void op_rg(float r, float g, float b);
	abstract public void op_K(float c, float m, float y, float k);
	abstract public void op_k(float c, float m, float y, float k);

	abstract public void op_BI(Image image);
	abstract public void op_sh(String name, Shade shade);
	abstract public void op_Do_image(String name, Image image);
	abstract public void op_Do_form(String name, PDFObject form, PDFObject pageResources);

	abstract public void op_MP(String tag);
	abstract public void op_DP(String tag, PDFObject raw);
	abstract public void op_BMC(String tag);
	abstract public void op_BDC(String tag, PDFObject raw);
	abstract public void op_EMC();

	abstract public void op_BX();
	abstract public void op_EX();
}
