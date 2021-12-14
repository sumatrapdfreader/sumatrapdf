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

import java.lang.annotation.Native;

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
		return new LinkDestination(chapter, page, LINK_DEST_FIT, 0, 0, 0, 0, 0);
	}

	public static LinkDestination FitB(int chapter, int page)
	{
		return new LinkDestination(chapter, page, LINK_DEST_FIT_B, 0, 0, 0, 0, 0);
	}

	public static LinkDestination XYZ(int chapter, int page, float left, float top, float zoom)
	{
		return new LinkDestination(chapter, page, LINK_DEST_XYZ, left, top, 0, 0, zoom);
	}

	public static LinkDestination FitH(int chapter, int page, float top)
	{
		return new LinkDestination(chapter, page, LINK_DEST_FIT_H, 0, top, 0, 0, 0);
	}

	public static LinkDestination FitBH(int chapter, int page, float top)
	{
		return new LinkDestination(chapter, page, LINK_DEST_FIT_BH, 0, top, 0, 0, 0);
	}

	public static LinkDestination FitV(int chapter, int page, float left)
	{
		return new LinkDestination(chapter, page, LINK_DEST_FIT_V, left, 0, 0, 0, 0);
	}

	public static LinkDestination FitBV(int chapter, int page, float left)
	{
		return new LinkDestination(chapter, page, LINK_DEST_FIT_BV, left, 0, 0, 0, 0);
	}

	public static LinkDestination FitR(int chapter, int page, float left, float top, float width, float height)
	{
		return new LinkDestination(chapter, page, LINK_DEST_FIT_R, left, top, width, height, 0);
	}
}
