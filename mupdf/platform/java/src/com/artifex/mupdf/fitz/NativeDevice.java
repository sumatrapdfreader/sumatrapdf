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

package com.artifex.mupdf.fitz;

public class NativeDevice extends Device
{
	static {
		Context.init();
	}

	private long nativeInfo;
	private Object nativeResource;

	protected native void finalize();

	public void destroy() {
		finalize();
	}

	protected NativeDevice(long p) {
		super(p);
	}

	public native final void close();

	public native final void fillPath(Path path, boolean evenOdd, Matrix ctm, ColorSpace cs, float[] color, float alpha, int cp);
	public native final void strokePath(Path path, StrokeState stroke, Matrix ctm, ColorSpace cs, float[] color, float alpha, int cp);
	public native final void clipPath(Path path, boolean evenOdd, Matrix ctm);
	public native final void clipStrokePath(Path path, StrokeState stroke, Matrix ctm);

	public native final void fillText(Text text, Matrix ctm, ColorSpace cs, float[] color, float alpha, int cp);
	public native final void strokeText(Text text, StrokeState stroke, Matrix ctm, ColorSpace cs, float[] color, float alpha, int cp);
	public native final void clipText(Text text, Matrix ctm);
	public native final void clipStrokeText(Text text, StrokeState stroke, Matrix ctm);
	public native final void ignoreText(Text text, Matrix ctm);

	public native final void fillShade(Shade shd, Matrix ctm, float alpha, int cp);
	public native final void fillImage(Image img, Matrix ctm, float alpha, int cp);
	public native final void fillImageMask(Image img, Matrix ctm, ColorSpace cs, float[] color, float alpha, int cp);
	public native final void clipImageMask(Image img, Matrix ctm);

	public native final void popClip();

	public native final void beginMask(Rect rect, boolean luminosity, ColorSpace cs, float[] bc, int cp);
	public native final void endMask();
	public native final void beginGroup(Rect rect, ColorSpace cs, boolean isolated, boolean knockout, int blendmode, float alpha);
	public native final void endGroup();

	public native final int beginTile(Rect area, Rect view, float xstep, float ystep, Matrix ctm, int id);
	public native final void endTile();

	public native final void renderFlags(int set, int clear);
	public native final void setDefaultColorSpaces(DefaultColorSpaces dcs);

	public native final void beginLayer(String name);
	public native final void endLayer();

	public native final void beginStructure(int standard, String raw, int uid);
	public native final void endStructure();

	public native final void beginMetatext(int meta, String text);
	public native final void endMetatext();
}
