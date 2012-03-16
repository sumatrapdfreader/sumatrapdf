package com.artifex.mupdf;

import android.graphics.RectF;

public class LinkInfo extends RectF {
	public int pageNumber;

	public LinkInfo(float l, float t, float r, float b, int p) {
		super(l, t, r, b);
		pageNumber = p;
	}
}
