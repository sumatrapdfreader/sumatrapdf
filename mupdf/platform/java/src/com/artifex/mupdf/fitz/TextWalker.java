package com.artifex.mupdf.fitz;

public interface TextWalker
{
	void showGlyph(Font font, Matrix trm, int glyph, int unicode, boolean wmode);
}
