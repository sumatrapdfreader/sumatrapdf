package com.artifex.mupdf.fitz;

public class PDFGraftMap
{
	static {
		Context.init();
	}

	private long pointer;

	protected native void finalize();

	public void destroy() {
		finalize();
	}

	public native PDFObject graftObject(PDFObject obj);

	public native void graftPage(int pageTo, PDFDocument src, int pageFrom);

	private PDFGraftMap(long p) {
		pointer = p;
	}
}
