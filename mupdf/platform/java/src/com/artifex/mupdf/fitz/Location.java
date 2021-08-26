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

public final class Location
{
	public final int chapter;
	public final int page;
	public final float x, y;

	public Location(int chapter, int page) {
		this.chapter = chapter;
		this.page = page;
		this.x = this.y = 0;
	}

	public Location(int chapter, int page, float x, float y) {
		this.chapter = chapter;
		this.page = page;
		this.x = x;
		this.y = y;
	}

	public Location(Location location, float x, float y) {
		this.chapter = location.chapter;
		this.page = location.page;
		this.x = x;
		this.y = y;
	}

	public boolean equals(Object obj) {
		if (!(obj instanceof Location))
			return false;

		Location other = (Location) obj;

		return this.chapter == other.chapter &&
			this.page == other.page &&
			this.x == other.x &&
			this.y == other.y;
	}
}
