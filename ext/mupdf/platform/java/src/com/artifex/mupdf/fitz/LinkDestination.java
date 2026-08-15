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

import java.lang.annotation.Native;

import java.lang.Float;

public class LinkDestination extends Location
{
	/* Destinations, keep in sync with FZ_LINK_DEST_* */
	@Native public static final int LINK_DEST_FIT = 0;
	@Native public static final int LINK_DEST_FIT_B = 1;
	@Native public static final int LINK_DEST_FIT_H = 2;
	@Native public static final int LINK_DEST_FIT_BH = 3;
	@Native public static final int LINK_DEST_FIT_V = 4;
	@Native public static final int LINK_DEST_FIT_BV = 5;
	@Native public static final int LINK_DEST_FIT_R = 6;
	@Native public static final int LINK_DEST_XYZ = 7;

	public int type;
	public float x;
	public float y;
	public float width;
	public float height;
	public float zoom;

	public LinkDestination(int chapter, int page, int type, float x, float y, float w, float h, float zoom)
	{
		super(chapter, page);
		this.type = type;
		this.x = x;
		this.y = y;
		this.width = width;
		this.height = height;
		this.zoom = zoom;
	}

	public static LinkDestination Fit(int chapter, int page)
	{
		return new LinkDestination(chapter, page, LINK_DEST_FIT, Float.NaN, Float.NaN, Float.NaN, Float.NaN, Float.NaN);
	}

	public static LinkDestination FitB(int chapter, int page)
	{
		return new LinkDestination(chapter, page, LINK_DEST_FIT_B, Float.NaN, Float.NaN, Float.NaN, Float.NaN, Float.NaN);
	}

	public static LinkDestination XYZ(int chapter, int page)
	{
		return XYZ(chapter, page, Float.NaN, Float.NaN, Float.NaN);
	}
	public static LinkDestination XYZ(int chapter, int page, float zoom)
	{
		return XYZ(chapter, page, Float.NaN, Float.NaN, zoom);
	}
	public static LinkDestination XYZ(int chapter, int page, float x, float y)
	{
		return XYZ(chapter, page, x, y, Float.NaN);
	}
	public static LinkDestination XYZ(int chapter, int page, float x, float y, float zoom)
	{
		return new LinkDestination(chapter, page, LINK_DEST_XYZ, x, y, Float.NaN, Float.NaN, zoom);
	}

	public static LinkDestination FitH(int chapter, int page)
	{
		return FitH(chapter, page, Float.NaN);
	}
	public static LinkDestination FitH(int chapter, int page, float y)
	{
		return new LinkDestination(chapter, page, LINK_DEST_FIT_H, Float.NaN, y, Float.NaN, Float.NaN, Float.NaN);
	}

	public static LinkDestination FitBH(int chapter, int page)
	{
		return FitBH(chapter, page, Float.NaN);
	}
	public static LinkDestination FitBH(int chapter, int page, float y)
	{
		return new LinkDestination(chapter, page, LINK_DEST_FIT_BH, Float.NaN, y, Float.NaN, Float.NaN, Float.NaN);
	}

	public static LinkDestination FitV(int chapter, int page)
	{
		return FitV(chapter, page, Float.NaN);
	}
	public static LinkDestination FitV(int chapter, int page, float x)
	{
		return new LinkDestination(chapter, page, LINK_DEST_FIT_V, x, Float.NaN, Float.NaN, Float.NaN, Float.NaN);
	}

	public static LinkDestination FitBV(int chapter, int page)
	{
		return FitBV(chapter, page, Float.NaN);
	}
	public static LinkDestination FitBV(int chapter, int page, float x)
	{
		return new LinkDestination(chapter, page, LINK_DEST_FIT_BV, x, Float.NaN, Float.NaN, Float.NaN, Float.NaN);
	}

	public static LinkDestination FitR(int chapter, int page, float x, float y, float width, float height)
	{
		return new LinkDestination(chapter, page, LINK_DEST_FIT_R, x, y, width, height, Float.NaN);
	}

	public boolean hasX() { return !Float.isNaN(x); }
	public boolean hasY() { return !Float.isNaN(y); }
	public boolean hasZoom() { return !Float.isNaN(zoom) && zoom != 0; }
	public boolean hasWidth() { return !Float.isNaN(width); }
	public boolean hasHeight() { return !Float.isNaN(height); }
}
