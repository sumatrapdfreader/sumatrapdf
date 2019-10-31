package com.artifex.mupdf.fitz;

import java.util.ArrayList;

public class StructuredText
{
	static {
		Context.init();
	}

	public static final int SELECT_CHARS = 0;
	public static final int SELECT_WORDS = 1;
	public static final int SELECT_LINES = 2;

	private long pointer;

	protected native void finalize();

	public void destroy() {
		finalize();
		pointer = 0;
	}

	private StructuredText(long p) {
		pointer = p;
	}

	public native Quad[] search(String needle);
	public native Quad[] highlight(Point a, Point b);
	public native Quad snapSelection(Point a, Point b, int mode);
	public native String copy(Point a, Point b);

	public native void walk(StructuredTextWalker walker);

	public TextBlock[] getBlocks() {
		BlockWalker walker = new BlockWalker();
		walk(walker);
		return walker.getBlocks();
	}

	class BlockWalker implements StructuredTextWalker {
		ArrayList<TextBlock> blocks;
		ArrayList<TextLine> lines;
		ArrayList<TextChar> chrs;
		Rect lineBbox;
		Rect blockBbox;

		BlockWalker() {
			blocks = new ArrayList<TextBlock>();
		}

		public void onImageBlock(Rect bbox, Matrix transform, Image image) {
		}

		public void beginTextBlock(Rect bbox) {
			lines = new ArrayList<TextLine>();
			blockBbox = bbox;
		}

		public void endTextBlock() {
			TextBlock block = new TextBlock();
			block.bbox = blockBbox;
			block.lines = lines.toArray(new TextLine[0]);
			blocks.add(block);
		}

		public void beginLine(Rect bbox, int wmode) {
			chrs = new ArrayList<TextChar>();
			lineBbox = bbox;
		}

		public void endLine() {
			TextLine line = new TextLine();
			line.bbox = lineBbox;
			line.chars =  chrs.toArray(new TextChar[0]);
			lines.add(line);
		}

		public void onChar(int c, Point origin, Font font, float size, Quad quad) {
			TextChar chr = new TextChar();
			chr.c = c;
			chr.quad = quad;
			chrs.add(chr);
		}

		TextBlock[] getBlocks() {
			return blocks.toArray(new TextBlock[0]);
		}
	}

	public class TextBlock {
		public TextLine[] lines;
		public Rect bbox;
	}

	public class TextLine {
		public TextChar[] chars;
		public Rect bbox;
	}

	public class TextChar {
		public int c;
		public Quad quad;
		public boolean isWhitespace() {
			return Character.isWhitespace(c);
		}
	}

}
