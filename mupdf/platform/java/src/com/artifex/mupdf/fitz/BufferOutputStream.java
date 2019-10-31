package com.artifex.mupdf.fitz;

import java.io.IOException;
import java.io.OutputStream;

public class BufferOutputStream extends OutputStream
{
	protected Buffer buffer;
	protected int position;
	protected int resetPosition;

	public BufferOutputStream(Buffer buffer) {
		super();
		this.buffer = buffer;
		this.position = 0;
	}

	public void write(byte[] b) {
		buffer.writeBytes(b);
	}

	public void write(byte[] b, int off, int len) {
		buffer.writeBytesFrom(b, off, len);
	}

	public void write(int b) {
		buffer.writeByte((byte) b);
	}
}
