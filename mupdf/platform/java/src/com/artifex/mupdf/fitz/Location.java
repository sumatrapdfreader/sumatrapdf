// Copyright (C) 2004-2022 Artifex Software, Inc.
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

import java.util.Objects;

public class Location
{
	public final int chapter;
	public final int page;

	public Location(int chapter, int page) {
		this.chapter = chapter;
		this.page = page;
	}

	@Override
	public boolean equals(Object obj) {
		if (!(obj instanceof Location))
			return false;
		Location other = (Location) obj;
		return this.chapter == other.chapter && this.page == other.page;
	}

	@Override
	public int hashCode() {
		return Objects.hash(this.chapter, this.page);
	}

	public String toString() {
		StringBuilder sb = new StringBuilder();
		sb.append("Location(chapter=");
		sb.append(chapter);
		sb.append(", page=");
		sb.append(page);
		sb.append(")");
		return sb.toString();
	}
}
