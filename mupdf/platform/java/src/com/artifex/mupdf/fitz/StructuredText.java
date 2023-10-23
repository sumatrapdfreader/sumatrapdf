// Copyright (C) 2004-2021 Artifex Software, Inc.
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
	}

	private StructuredText(long p) {
		pointer = p;
	}

	public native Quad[][] search(String needle);
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
		Point lineDir;
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

		public void beginLine(Rect bbox, int wmode, Point dir) {
			chrs = new ArrayList<TextChar>();
			lineBbox = bbox;
			lineDir = dir;
		}

		public void endLine() {
			TextLine line = new TextLine();
			line.bbox = lineBbox;
			line.dir = lineDir;
			line.chars =  chrs.toArray(new TextChar[0]);
			lines.add(line);
		}

		public void onChar(int c, Point origin, Font font, float size, Quad quad) {
			TextChar chr = new TextChar();
			chr.c = c;
			chr.quad = quad;
			chr.origin = origin;
			chrs.add(chr);
		}

		TextBlock[] getBlocks() {
			return blocks.toArray(new TextBlock[0]);
		}
	}

	public static class TextBlock {
		public TextLine[] lines;
		public Rect bbox;
	}

	public static class TextLine {
		public TextChar[] chars;
		public Rect bbox;
		public Point dir;
	}

	public static class TextChar {
		public int c;
		public Quad quad;
		public Point origin;
		public boolean isWhitespace() {
			return Character.isWhitespace(c);
		}
	}

}
