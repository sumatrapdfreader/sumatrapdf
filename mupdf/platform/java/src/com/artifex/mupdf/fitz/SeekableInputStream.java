package com.artifex.mupdf.fitz;

import java.io.IOException;

public interface SeekableInputStream extends SeekableStream
{
	/* Behaves like java.io.InputStream.read */
	int read(byte[] b) throws IOException;
}
