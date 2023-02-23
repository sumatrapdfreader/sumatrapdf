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
// Artifex Software, Inc., 1305 Grant Avenue - Suite 200, Novato,
// CA 94945, U.S.A., +1(415)492-9861, for further information.

package com.artifex.mupdf.fitz;

abstract public class Device
{
	static {
		Context.init();
	}

	protected long pointer;

	protected native void finalize();

	public void destroy() {
		finalize();
	}

	private native long newNative();

	protected Device() {
		pointer = newNative();
	}

	protected Device(long p) {
		pointer = p;
	}

	/* To implement your own device in Java, you should define your own
	 * class that extends Device, and override as many of the following
	 * functions as is appropriate. For example:
	 *
	 * class ImageTraceDevice extends Device
	 * {
	 *	void fillImage(Image img, Matrix ctx, float alpha) {
	 *		System.out.println("Image!");
	 *	}
	 * };
	 */

	abstract public void close();
	abstract public void fillPath(Path path, boolean evenOdd, Matrix ctm, ColorSpace cs, float[] color, float alpha, int cp);
	abstract public void strokePath(Path path, StrokeState stroke, Matrix ctm, ColorSpace cs, float[] color, float alpha, int cp);
	abstract public void clipPath(Path path, boolean evenOdd, Matrix ctm);
	abstract public void clipStrokePath(Path path, StrokeState stroke, Matrix ctm);
	abstract public void fillText(Text text, Matrix ctm, ColorSpace cs, float[] color, float alpha, int cp);
	abstract public void strokeText(Text text, StrokeState stroke, Matrix ctm, ColorSpace cs, float[] color, float alpha, int cp);
	abstract public void clipText(Text text, Matrix ctm);
	abstract public void clipStrokeText(Text text, StrokeState stroke, Matrix ctm);
	abstract public void ignoreText(Text text, Matrix ctm);
	abstract public void fillShade(Shade shd, Matrix ctm, float alpha, int cp);
	abstract public void fillImage(Image img, Matrix ctm, float alpha, int cp);
	abstract public void fillImageMask(Image img, Matrix ctm, ColorSpace cs, float[] color, float alpha, int cp);
	abstract public void clipImageMask(Image img, Matrix ctm);
	abstract public void popClip();
	abstract public void beginMask(Rect area, boolean luminosity, ColorSpace cs, float[] bc, int cp);
	abstract public void endMask();
	abstract public void beginGroup(Rect area, ColorSpace cs, boolean isolated, boolean knockout, int blendmode, float alpha);
	abstract public void endGroup();
	abstract public int beginTile(Rect area, Rect view, float xstep, float ystep, Matrix ctm, int id);
	abstract public void endTile();
	abstract public void renderFlags(int set, int clear);
	abstract public void setDefaultColorSpaces(DefaultColorSpaces dcs);
	abstract public void beginLayer(String name);
	abstract public void endLayer();
	abstract public void beginStructure(int standard, String raw, int uid);
	abstract public void endStructure();
	abstract public void beginMetatext(int meta, String text);
	abstract public void endMetatext();

	/* PDF 1.4 -- standard separable */
	public static final int BLEND_NORMAL = 0;
	public static final int BLEND_MULTIPLY = 1;
	public static final int BLEND_SCREEN = 2;
	public static final int BLEND_OVERLAY = 3;
	public static final int BLEND_DARKEN = 4;
	public static final int BLEND_LIGHTEN = 5;
	public static final int BLEND_COLOR_DODGE = 6;
	public static final int BLEND_COLOR_BURN = 7;
	public static final int BLEND_HARD_LIGHT = 8;
	public static final int BLEND_SOFT_LIGHT = 9;
	public static final int BLEND_DIFFERENCE = 10;
	public static final int BLEND_EXCLUSION = 11;

	/* PDF 1.4 -- standard non-separable */
	public static final int BLEND_HUE = 12;
	public static final int BLEND_SATURATION = 13;
	public static final int BLEND_COLOR = 14;
	public static final int BLEND_LUMINOSITY = 15;

	public static final int DEVICE_FLAG_MASK = 1;
	public static final int DEVICE_FLAG_COLOR = 2;
	public static final int DEVICE_FLAG_UNCACHEABLE = 4;
	public static final int DEVICE_FLAG_FILLCOLOR_UNDEFINED = 8;
	public static final int DEVICE_FLAG_STROKECOLOR_UNDEFINED = 16;
	public static final int DEVICE_FLAG_STARTCAP_UNDEFINED = 32;
	public static final int DEVICE_FLAG_DASHCAP_UNDEFINED = 64;
	public static final int DEVICE_FLAG_ENDCAP_UNDEFINED = 128;
	public static final int DEVICE_FLAG_LINEJOIN_UNDEFINED = 256;
	public static final int DEVICE_FLAG_MITERLIMIT_UNDEFINED = 512;
	public static final int DEVICE_FLAG_LINEWIDTH_UNDEFINED = 1024;
	public static final int DEVICE_FLAG_BBOX_DEFINED = 2048;
	public static final int DEVICE_FLAG_GRIDFIT_AS_TILED = 4096;

	public static final int STRUCTURE_INVALID = -1;
	public static final int STRUCTURE_DOCUMENT = 0;
	public static final int STRUCTURE_PART = 1;
	public static final int STRUCTURE_ART = 2;
	public static final int STRUCTURE_SECT = 3;
	public static final int STRUCTURE_DIV = 4;
	public static final int STRUCTURE_BLOCKQUOTE = 5;
	public static final int STRUCTURE_CAPTION = 6;
	public static final int STRUCTURE_TOC = 7;
	public static final int STRUCTURE_TOCI = 8;
	public static final int STRUCTURE_INDEX = 9;
	public static final int STRUCTURE_NONSTRUCT = 10;
	public static final int STRUCTURE_PRIVATE = 11;
	public static final int STRUCTURE_P = 12;
	public static final int STRUCTURE_H = 13;
	public static final int STRUCTURE_H1 = 14;
	public static final int STRUCTURE_H2 = 15;
	public static final int STRUCTURE_H3 = 16;
	public static final int STRUCTURE_H4 = 17;
	public static final int STRUCTURE_H5 = 18;
	public static final int STRUCTURE_H6 = 19;
	public static final int STRUCTURE_LIST = 20;
	public static final int STRUCTURE_LISTITEM = 21;
	public static final int STRUCTURE_LABEL = 22;
	public static final int STRUCTURE_LISTBODY = 23;
	public static final int STRUCTURE_TABLE = 24;
	public static final int STRUCTURE_TR = 25;
	public static final int STRUCTURE_TH = 26;
	public static final int STRUCTURE_TD = 27;
	public static final int STRUCTURE_THEAD = 28;
	public static final int STRUCTURE_TBODY = 29;
	public static final int STRUCTURE_TFOOT = 30;
	public static final int STRUCTURE_SPAN = 31;
	public static final int STRUCTURE_QUOTE = 32;
	public static final int STRUCTURE_NOTE = 33;
	public static final int STRUCTURE_REFERENCE = 34;
	public static final int STRUCTURE_BIBENTRY = 35;
	public static final int STRUCTURE_CODE = 36;
	public static final int STRUCTURE_LINK = 37;
	public static final int STRUCTURE_ANNOT = 38;
	public static final int STRUCTURE_RUBY = 39;
	public static final int STRUCTURE_RB = 40;
	public static final int STRUCTURE_RT = 41;
	public static final int STRUCTURE_RP = 42;
	public static final int STRUCTURE_WARICHU = 43;
	public static final int STRUCTURE_WT = 44;
	public static final int STRUCTURE_WP = 45;
	public static final int STRUCTURE_FIGURE = 46;
	public static final int STRUCTURE_FORMULA = 47;
	public static final int STRUCTURE_FORM = 48;

	public static final int METATEXT_ACTUALTEXT = 0;
	public static final int METATEXT_ALT = 1;
	public static final int METATEXT_ABBREVIATION = 2;
	public static final int METATEXT_TITLE = 3;
}
