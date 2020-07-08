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
