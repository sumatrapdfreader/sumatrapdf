package com.artifex.mupdf.fitz;

import java.io.IOException;

public interface SeekableOutputStream extends SeekableStream
{
	/* Behaves like java.io.OutputStream.write */
	void write(byte[] b, int off, int len) throws IOException;
}
