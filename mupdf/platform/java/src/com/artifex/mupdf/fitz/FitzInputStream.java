package com.artifex.mupdf.fitz;

import java.io.IOException;
import java.io.InputStream;

public class FitzInputStream extends InputStream
{
	static {
		Context.init();
	}

	private long pointer;
	private long markpos;
	private boolean closed;

	protected native void finalize();

	public void destroy() {
		finalize();
	}

	private FitzInputStream(long p) {
		pointer = p;
		markpos = -1;
		closed = false;
	}

	public native void mark(int readlimit);
	public native boolean markSupported();
	public native void reset() throws IOException;

	public native int available();

	private native int readByte();
	private native int readArray(byte[] b, int off, int len);
	public int read() {
		return readByte();
	}
	public int read(byte[] b, int off, int len) {
		return readArray(b, off, len);
	}
	public int read(byte[] b) {
		return readArray(b, 0, b.length);
	}

	public native void close() throws IOException;
}
