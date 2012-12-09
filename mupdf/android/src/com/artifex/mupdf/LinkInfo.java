package com.artifex.mupdf;

import android.graphics.RectF;

public class LinkInfo {
	final public RectF rect;

	public LinkInfo(float l, float t, float r, float b) {
		rect = new RectF(l, t, r, b);
	}

	public void acceptVisitor(LinkInfoVisitor visitor) {
	}
}