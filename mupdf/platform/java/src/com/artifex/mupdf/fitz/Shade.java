package com.artifex.mupdf.fitz;

public class Shade
{
	static {
		Context.init();
	}

	private long pointer;

	protected native void finalize();

	public void destroy() {
		finalize();
		pointer = 0;
	}

	private Shade(long p) {
		pointer = p;
	}
}
