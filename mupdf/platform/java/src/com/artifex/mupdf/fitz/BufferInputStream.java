package com.artifex.mupdf.fitz;

import java.io.IOException;
import java.io.InputStream;

public class BufferInputStream extends InputStream
{
	protected Buffer buffer;
	protected int position;
	protected int resetPosition;

	public BufferInputStream(Buffer buffer) {
		super();
		this.buffer = buffer;
		this.position = 0;
		this.resetPosition = -1;
	}

	public int available() {
		return buffer.getLength();
	}

	public void mark(int readlimit) {
		resetPosition = position;
	}

	public boolean markSupported() {
		return true;
	}

	public int read() {
		int c = buffer.readByte(position);
		if (c >= 0)
			position++;
		return c;
	}

	public int read(byte[] b) {
		int n = buffer.readBytes(position, b);
		if (n >= 0)
			position += n;
		return n;
	}

	public int read(byte[] b, int off, int len) {
		int n = buffer.readBytesInto(position, b, off, len);
		if (n >= 0)
			position += n;
		return n;
	}

	public void reset() throws IOException {
		if (resetPosition < 0)
			throw  new IOException("cannot reset because mark never set");
		if (resetPosition >= buffer.getLength())
			throw  new IOException("cannot reset because mark set outside of buffer");

		position = resetPosition;
	}
}
