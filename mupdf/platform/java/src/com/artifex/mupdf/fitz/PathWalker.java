package com.artifex.mupdf.fitz;

public interface PathWalker
{
	void moveTo(float x, float y);
	void lineTo(float x, float y);
	void curveTo(float cx1, float cy1, float cx2, float cy2, float ex, float ey);
	void closePath();
}
