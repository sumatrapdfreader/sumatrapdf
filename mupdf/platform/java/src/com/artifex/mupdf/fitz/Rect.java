package com.artifex.mupdf.fitz;

public class Rect
{
	static {
		Context.init();
	}

	public float x0;
	public float y0;
	public float x1;
	public float y1;

	// Minimum and Maximum values that can survive round trip
	// from int to float.
	private static final int FZ_MIN_INF_RECT = 0x80000000;
	private static final int FZ_MAX_INF_RECT = 0x7fffff80;

	public Rect()
	{
		// Invalid (hence zero area) rectangle. Unioning
		// this with any rectangle (or point) will 'cure' it
		x0 = y0 = FZ_MAX_INF_RECT;
		x1 = y1 = FZ_MIN_INF_RECT;
	}

	public Rect(float x0, float y0, float x1, float y1)
	{
		this.x0 = x0;
		this.y0 = y0;
		this.x1 = x1;
		this.y1 = y1;
	}

	public Rect(Quad q)
	{
		this.x0 = q.ll_x;
		this.y0 = q.ll_y;
		this.x1 = q.ur_x;
		this.y1 = q.ur_y;
	}

	public Rect(Rect r)
	{
		this(r.x0, r.y0, r.x1, r.y1);
	}

	public Rect(RectI r)
	{
		this(r.x0, r.y0, r.x1, r.y1);
	}

	public native void adjustForStroke(StrokeState state, Matrix ctm);

	public String toString()
	{
		return "[" + x0 + " " + y0 + " " + x1 + " " + y1 + "]";
	}

	public boolean isInfinite()
	{
		return this.x0 == FZ_MIN_INF_RECT &&
			this.y0 == FZ_MIN_INF_RECT &&
			this.x1 == FZ_MAX_INF_RECT &&
			this.y1 == FZ_MAX_INF_RECT;
	}

	public Rect transform(Matrix tm)
	{
		if (this.isInfinite())
			return this;
		if (!this.isValid())
			return this;

		float ax0 = x0 * tm.a;
		float ax1 = x1 * tm.a;

		if (ax0 > ax1)
		{
			float t = ax0;
			ax0 = ax1;
			ax1 = t;
		}

		float cy0 = y0 * tm.c;
		float cy1 = y1 * tm.c;

		if (cy0 > cy1)
		{
			float t = cy0;
			cy0 = cy1;
			cy1 = t;
		}
		ax0 += cy0 + tm.e;
		ax1 += cy1 + tm.e;

		float bx0 = x0 * tm.b;
		float bx1 = x1 * tm.b;

		if (bx0 > bx1)
		{
			float t = bx0;
			bx0 = bx1;
			bx1 = t;
		}

		float dy0 = y0 * tm.d;
		float dy1 = y1 * tm.d;

		if (dy0 > dy1)
		{
			float t = dy0;
			dy0 = dy1;
			dy1 = t;
		}
		bx0 += dy0 + tm.f;
		bx1 += dy1 + tm.f;

		x0 = ax0;
		x1 = ax1;
		y0 = bx0;
		y1 = bx1;

		return this;
	}

	public boolean contains(float x, float y)
	{
		if (isEmpty())
			return false;

		return (x >= x0 && x < x1 && y >= y0 && y < y1);
	}

	public boolean contains(Rect r)
	{
		if (isEmpty() || r.isEmpty())
			return false;

		return (r.x0 >= x0 && r.x1 <= x1 && r.y0 >= y0 && r.y1 <= y1);
	}

	public boolean isEmpty()
	{
		return (x0 >= x1 || y0 >= y1);
	}

	public boolean isValid()
	{
		return (x0 <= x1 || y0 <= y1);
	}

	public void union(Rect r)
	{
		if (!r.isValid() || this.isInfinite())
			return;
		if (!this.isValid() || r.isInfinite())
		{
			x0 = r.x0;
			y0 = r.y0;
			x1 = r.x1;
			y1 = r.y1;
			return;
		}

			if (r.x0 < x0)
				x0 = r.x0;
			if (r.y0 < y0)
				y0 = r.y0;
			if (r.x1 > x1)
				x1 = r.x1;
			if (r.y1 > y1)
				y1 = r.y1;
		}
}
