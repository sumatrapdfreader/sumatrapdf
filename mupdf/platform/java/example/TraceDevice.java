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

package example;

import com.artifex.mupdf.fitz.*;

public class TraceDevice extends Device implements PathWalker, TextWalker
{
	public String traceColor(ColorSpace cs, float[] color, float alpha) {
		String s = cs + " [";
		int i;
		for (i = 0; i < color.length; ++i) {
			if (i > 0) s += " ";
			s += color[i];
		}
		return s + "] " + alpha;
	}
	public String traceStroke(StrokeState stroke) {
		return "c=" + stroke.getStartCap() + "," + stroke.getDashCap() + "," + stroke.getEndCap() +
			" j=" + stroke.getLineJoin() +
			" m=" + stroke.getMiterLimit() +
			" l=" + stroke.getLineWidth();
	}

	public void moveTo(float x, float y) {
		System.out.println("moveto " + x + " " + y);
	}

	public void lineTo(float x, float y) {
		System.out.println("lineto " + x + " " + y);
	}

	public void curveTo(float cx1, float cy1, float cx2, float cy2, float ex, float ey) {
		System.out.println("curveto " + cx1 + " " + cy1 + " " + cx2 + " " + cy2 + " " + ex + " " + ey);
	}

	public void closePath() {
		System.out.println("closepath");
	}

	public void showGlyph(Font font, Matrix trm, int glyph, int unicode, boolean wmode) {
		System.out.println("glyph '" + (char)unicode + "' " + glyph + "\t" + font + " " + trm);
	}

	public void tracePath(Path path) {
		path.walk(this);
	}

	public void traceText(Text text) {
		text.walk(this);
	}

	public void close() {
	}

	public void fillPath(Path path, boolean evenOdd, Matrix ctm, ColorSpace cs, float[] color, float alpha, int cp) {
		System.out.println("fillPath " + evenOdd + " " + ctm + " " + traceColor(cs, color, alpha));
		tracePath(path);
	}

	public void strokePath(Path path, StrokeState stroke, Matrix ctm, ColorSpace cs, float[] color, float alpha, int cp) {
		System.out.println("strokePath " + traceStroke(stroke) + " " + ctm + " " + traceColor(cs, color, alpha));
		tracePath(path);
	}

	public void clipPath(Path path, boolean evenOdd, Matrix ctm) {
		System.out.println("clipPath " + evenOdd + " " + ctm);
		tracePath(path);
	}

	public void clipStrokePath(Path path, StrokeState stroke, Matrix ctm) {
		System.out.println("clipStrokePath " + traceStroke(stroke) + " " + ctm);
		tracePath(path);
	}

	public void fillText(Text text, Matrix ctm, ColorSpace cs, float[] color, float alpha, int cp) {
		System.out.println("fillText " + ctm + " " + traceColor(cs, color, alpha));
		traceText(text);
	}

	public void strokeText(Text text, StrokeState stroke, Matrix ctm, ColorSpace cs, float[] color, float alpha, int cp) {
		System.out.println("strokeText " + ctm + " " + traceStroke(stroke) + " " + traceColor(cs, color, alpha));
		traceText(text);
	}

	public void clipText(Text text, Matrix ctm) {
		System.out.println("clipText " + ctm);
		traceText(text);
	}

	public void clipStrokeText(Text text, StrokeState stroke, Matrix ctm) {
		System.out.println("clipStrokeText " + ctm + " " + traceStroke(stroke));
		traceText(text);
	}

	public void ignoreText(Text text, Matrix ctm) {
		System.out.println("ignoreText " + ctm);
		traceText(text);
	}

	public void fillShade(Shade shd, Matrix ctm, float alpha, int cp) {
		System.out.println("fillShade " + ctm + " " + alpha);
	}

	public void fillImage(Image img, Matrix ctm, float alpha, int cp) {
		System.out.println("fillImage " + ctm + " " + alpha);
	}

	public void fillImageMask(Image img, Matrix ctm, ColorSpace cs, float[] color, float alpha, int cp) {
		System.out.println("fillImageMask " + ctm + " " + traceColor(cs, color, alpha));
	}

	public void clipImageMask(Image img, Matrix ctm) {
		System.out.println("clipImageMask " + ctm);
	}

	public void popClip() {
		System.out.println("popClip");
	}

	public void beginMask(Rect rect, boolean luminosity, ColorSpace cs, float[] bc, int cp) {
		System.out.println("beginMask r=" + rect +
				" l=" + luminosity +
				" " + traceColor(cs, bc, 1));
	}

	public void endMask() {
		System.out.println("endMask");
	}

	public void beginGroup(Rect rect, ColorSpace cs, boolean isolated, boolean knockout, int blendmode, float alpha) {
		System.out.println("beginGroup r=" + rect +
				" i=" + isolated +
				" k=" + knockout +
				" bm=" + blendmode +
				" a=" + alpha);
	}

	public void endGroup() {
		System.out.println("endGroup");
	}

	public int beginTile(Rect area, Rect view, float xstep, float ystep, Matrix ctm, int id) {
		System.out.println("beginTile");
		return 0;
	}

	public void endTile() {
		System.out.println("endTile");
	}

	public void renderFlags(int set, int clear) {
		System.out.println("renderFlags set=" + set + " clear=" + clear);
	}

	public void setDefaultColorSpaces(DefaultColorSpaces dcs) {
		System.out.println("setDefaultColorSpaces" +
			" gray=" + dcs.getDefaultGray() +
			" rgb=" + dcs.getDefaultRGB() +
			" cmyk=" + dcs.getDefaultCMYK() +
			" outputIntent=" + dcs.getOutputIntent());
	}

	public void beginLayer(String name) {
		System.out.println("beginLayer");
	}

	public void endLayer() {
		System.out.println("endLayer");
	}

	public void beginStructure(int standard, String raw, int uid) {
		System.out.println("beginStructure standard=" + standard +
			" raw=" + raw +
			" uid=" + uid);
	}

	public void endStructure() {
		System.out.println("endStructure");
	}

	public void beginMetatext(int meta, String text) {
		System.out.println("beginMetatext type=" + meta +
			" text=" + text);
	}

	public void endMetatext() {
		System.out.println("endMetatext");
	}

	public static void main(String[] args) {
		Document doc = Document.openDocument("pdfref17.pdf");
		Page page = doc.loadPage(1144);
		TraceDevice dev = new TraceDevice();
		page.run(dev, new Matrix());
	}
}
