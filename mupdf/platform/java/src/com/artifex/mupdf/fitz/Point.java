package com.artifex.mupdf.fitz;

public class Point
{
	public float x;
	public float y;

	public Point(float x, float y) {
		this.x = x;
		this.y = y;
	}

	public Point(Point p) {
		this.x = p.x;
		this.y = p.y;
	}

	public String toString() {
		return "[" + x + " " + y + "]";
	}

	public Point transform(Matrix tm) {
		float old_x = this.x;

		this.x = old_x * tm.a + y * tm.c + tm.e;
		this.y = old_x * tm.b + y * tm.d + tm.f;

		return this;
	}
}
