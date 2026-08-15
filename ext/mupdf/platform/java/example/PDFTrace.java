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

package example;

import com.artifex.mupdf.fitz.*;

public class PDFTrace extends PDFProcessor
{
	protected String formatFloatArray(float[] array, String suffix) {
		StringBuilder sb = new StringBuilder("[ ");
		int n = array.length;
		for (int i = 0; i < n; i++)
			sb.append(array[i] + " ");
		sb.append(']');
		if (suffix != null)
			sb.append(suffix);
		return sb.toString();
	}

	protected String formatString(String s, String suffix) {
		StringBuilder sb = new StringBuilder("(");
		int n = s.length();
		for (int i = 0; i < n; i++)
		{
			char c = s.charAt(i);
			if (c == '(' || c == ')' || c == '\\')
				sb.append('\\');
			sb.append(c);
		}
		sb.append(')');
		if (suffix != null)
			sb.append(suffix);
		return sb.toString();
	}

	protected String formatByteArray(byte[] array, String suffix) {
		StringBuilder sb = new StringBuilder("<");
		int n = array.length;
		for (int i = 0; i < n; i++)
			sb.append(String.format("%02x", array[i]));
		sb.append('>');
		if (suffix != null)
			sb.append(suffix);
		return sb.toString();

	}

	protected String formatObjectArray(Object[] array, String suffix)  {
		StringBuilder sb = new StringBuilder('[');
		int n = array.length;
		for (int i = 0; i < n; i++)
		{
			if (array[i] instanceof Float)
				sb.append(((Float) array[i]).toString() + " ");
			else if (array[i] instanceof String)
				sb.append(formatString((String) array[i], " "));
			else
				sb.append(formatByteArray((byte[]) array[i], " "));
		}
		sb.append("]");
		if (suffix != null)
			sb.append(suffix);
		return sb.toString();
	}

	public void pushResources(PDFObject resources) { }
	public void popResources() { }
	public void op_gs(String name, PDFObject extgstate) { }

	public void op_w(float lineWidth) { System.out.println(lineWidth+ " w"); }
	public void op_j(float lineJoin) { System.out.println(lineJoin + " j"); }
	public void op_J(float lineCap) { System.out.println(lineCap + " J"); }
	public void op_M(float miterLimit) { System.out.println(miterLimit + " M"); }
	public void op_d(float[] array, float phase) { System.out.print("[ "); for (int i = 0; i < array.length; i++) System.out.print(array[i] + " "); System.out.println("] " + phase + " d"); }
	public void op_ri(String intent) { System.out.println(intent + " ri"); }
	public void op_i(float flatness) { System.out.println(flatness + "i"); }

	public void op_q() { System.out.println("q"); }
	public void op_Q() { System.out.println("Q"); }
	public void op_cm(float a, float b, float c, float d, float e, float f) { System.out.println(a + " " + b + " " + c + " " + d + " " + e + " " + f + " cm"); }

	public void op_m(float x, float y) { System.out.println(x + " " + y + " m"); }
	public void op_l(float x, float y) { System.out.println(x + " " + y + " l"); }
	public void op_c(float x1, float y1, float x2, float y2, float x3, float y3) { System.out.println(x1 + " " + y1 + " " + x2 + " " + y2 + " " + x3 + " " + y3 + " c"); }
	public void op_v(float x2, float y2, float x3, float y3) { System.out.println(x2 + " " + y2 + " " + x3 + " " + y3 + " v"); }
	public void op_y(float x1, float y1, float x3, float y3) { System.out.println(x1 + " " + y1 + " " + x3 + " " + y3 + " f"); }
	public void op_h() { System.out.println("h"); }
	public void op_re(float x, float y, float w, float h) { System.out.println(x + " " + y + " " + w + " " + h + " re"); }

	public void op_S() { System.out.println("S"); }
	public void op_s() { System.out.println("s"); }
	public void op_F() { System.out.println("F"); }
	public void op_f() { System.out.println("f"); }
	public void op_fstar() { System.out.println("f*"); }
	public void op_B() { System.out.println("B"); }
	public void op_Bstar() { System.out.println("B*"); }
	public void op_b() { System.out.println("b"); }
	public void op_bstar() { System.out.println("b*"); }
	public void op_n() { System.out.println("n"); }

	public void op_W() { System.out.println("W"); }
	public void op_Wstar() { System.out.println("W*"); }

	public void op_BT() { System.out.println("BT"); }
	public void op_ET() { System.out.println("ET"); }

	public void op_Tc(float charspace) { System.out.println(charspace + " Tc"); }
	public void op_Tw(float wordspace) { System.out.println(wordspace + " Tw"); }
	public void op_Tz(float scale) { System.out.println(scale + " Tz"); }
	public void op_TL(float leading) { System.out.println(leading + " TL"); }
	public void op_Tf(String name, float size) { System.out.println("/" + name + " " + size + " Tf"); }
	public void op_Tr(float render) { System.out.println(render + " Tr"); }
	public void op_Ts(float rise) { System.out.println(rise + " Ts"); }

	public void op_Td(float tx, float ty) { System.out.println(tx + " " + ty + " Td"); }
	public void op_TD(float tx, float ty) { System.out.println(tx + " " + ty + " TD"); }
	public void op_Tm(float a, float b, float c, float d, float e, float f) { System.out.println(a + " " + b + " " + c + " " + d + " " + e + " " + f + " Tm"); }
	public void op_Tstar() { System.out.println("T*"); }

	public void op_TJ(Object[] array) { System.out.println(formatObjectArray(array, " TJ")); }
	public void op_Tj(byte[] array) { System.out.println(formatByteArray(array, " Tj")); }
	public void op_Tj(String string) { System.out.println(formatString(string, " Tj")); }
	public void op_squote(String text) { System.out.println(formatString(text, " '")); }
	public void op_squote(byte[] array) { System.out.println(formatByteArray(array, " '")); }
	public void op_dquote(float aw, float ac, String text) { System.out.println(aw + " " + ac + " " + formatString(text, " \"")); }
	public void op_dquote(float aw, float ac, byte[] array) { System.out.println(aw + " " + ac + " " + " " + formatByteArray(array, " \"")); }

	public void op_d0(float wx, float wy) { System.out.println(wx + " " + wy + " d0"); }
	public void op_d1(float wx, float wy, float llx, float lly, float urx, float ury) { System.out.println(wx + " " + wy + " " + llx + " " + lly + " " + urx + " " + ury + " d1"); }

	public void op_CS(String name, ColorSpace cs) { System.out.println("/" + name + " " + (cs != null ? cs.toString() : "null") + " CS"); }
	public void op_cs(String name, ColorSpace cs) { System.out.println("/" + name + " " + (cs != null ? cs.toString() : "null") + " cs"); }
	public void op_SC_color(float[] color) { for (int i = 0; i < color.length; i++) System.out.print(color[i] + " "); System.out.println("SC%color"); }
	public void op_sc_color(float[] color) { for (int i = 0; i < color.length; i++) System.out.print(color[i] + " "); System.out.println("sc%color"); }
	public void op_SC_pattern(String name, int idx, float[] color) { System.out.print("/" + name + " " + idx + " "); for (int i = 0; i < color.length; i++) System.out.print(color[i] + " "); System.out.println("SC%pattern"); }
	public void op_sc_pattern(String name, int idx, float[] color) { System.out.print("/" + name + " " + idx + " "); for (int i = 0; i < color.length; i++) System.out.print(color[i] + " "); System.out.println("sc%pattern"); }
	public void op_SC_shade(String name) { System.out.println("/" + name + " SC%shade"); }
	public void op_sc_shade(String name) { System.out.println("/" + name + " sc%shade"); }

	public void op_G(float g) { System.out.println(g + " G"); }
	public void op_g(float g) { System.out.println(g + " g"); }
	public void op_RG(float r, float g, float b) { System.out.println(r + " " + g + " " + b + " RG"); }
	public void op_rg(float r, float g, float b) { System.out.println(r + " " + g + " " + b + " rg"); }
	public void op_K(float c, float m, float y, float k) { System.out.println(c + " " + m + " " + y + " " + k + " K"); }
	public void op_k(float c, float m, float y, float k) { System.out.println(c + " " + m + " " + y + " " + k + " k"); }

	public void op_BI(Image image) { System.out.println("% BI ... ID ... EI"); }
	public void op_sh(String name, Shade shade) { System.out.println("/" + name + " sh"); }
	public void op_Do_image(String name, Image image) { System.out.println("/" + name + " Do%image"); }
	public void op_Do_form(String name, PDFObject form, PDFObject pageResources) { System.out.println("/" + name + " Do%form"); }

	public void op_MP(String tag) { System.out.println(tag + " MP"); }
	public void op_DP(String tag, PDFObject raw) { System.out.println(tag + " " + raw.toString() + " DP"); }
	public void op_BMC(String tag) { System.out.println("/" + tag + " BMC"); }
	public void op_BDC(String tag, PDFObject raw) { System.out.println("/" + tag + " " + raw.toString(true) + " BDC"); }
	public void op_EMC() { System.out.println("EMC"); }

	public void op_BX() { System.out.println("BX"); }
	public void op_EX() { System.out.println("EX"); }

	public static void main(String[] args) {
		if (args.length < 1)
		{
			System.err.println("Usage: PDFTrace pdffile [pages...]");
			return;
		}

		String filename = args[0];
		Document doc = Document.openDocument(args[0]);
		if (!doc.isPDF())
			System.err.println(args[0] + ": only PDF documents can be traced");
		PDFDocument pdf = doc.asPDF();
		PDFProcessor proc = new PDFTrace();

		if (args.length == 1)
		{
			int pageno = 1;
			PDFPage page = (PDFPage) pdf.loadPage(pageno - 1);
			System.out.println("--------PAGE " + pageno + " BEGIN--------");
			page.process(proc);
			System.out.println("--------PAGE " + pageno + " END----------");
		}
		else
		{
			for (int i = 1; i < args.length; i++)
			{
				int pageno = Integer.parseInt(args[i]);
				PDFPage page = (PDFPage) pdf.loadPage(pageno - 1);
				System.out.println("--------PAGE " + pageno + " BEGIN--------");
				page.process(proc);
				System.out.println("--------PAGE " + pageno + " END----------");
			}
		}
	}
}
