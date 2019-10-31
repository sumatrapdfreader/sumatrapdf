package com.artifex.mupdf.fitz;

import java.io.IOException;

public interface SeekableStream
{
	int SEEK_SET = 0; /* set file position to offset */
	int SEEK_CUR = 1; /* set file position to current location plus offset */
	int SEEK_END = 2; /* set file position to EOF plus offset */

	long seek(long offset, int whence) throws IOException;
	long position() throws IOException;
}
