package com.artifex.mupdf.fitz;

public interface StructuredTextWalker
{
	void onImageBlock(Rect bbox, Matrix transform, Image image);
	void beginTextBlock(Rect bbox);
	void endTextBlock();
	void beginLine(Rect bbox, int wmode);
	void endLine();
	void onChar(int c, Point origin, Font font, float size, Quad q);
}
