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

	private PDFGraftMap(long p) {
		pointer = p;
	}
}
