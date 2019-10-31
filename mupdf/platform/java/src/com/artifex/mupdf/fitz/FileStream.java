package com.artifex.mupdf.fitz;

import java.io.File;
import java.io.IOException;
import java.io.RandomAccessFile;

public class FileStream implements SeekableInputStream, SeekableOutputStream
{
	protected RandomAccessFile file;

	public FileStream(String path, String mode) throws IOException {
		file = new RandomAccessFile(path, mode);
	}

	public FileStream(File path, String mode) throws IOException {
		file = new RandomAccessFile(path, mode);
	}

	public int read(byte[] buf) throws IOException {
		return file.read(buf);
	}

	public void write(byte[] buf, int off, int len) throws IOException {
		file.write(buf, off, len);
	}

	public long seek(long offset, int whence) throws IOException {
		switch (whence) {
		case SEEK_SET: file.seek(offset); break;
		case SEEK_CUR: file.seek(file.getFilePointer() + offset); break;
		case SEEK_END: file.seek(file.length() + offset); break;
		}
		return file.getFilePointer();
	}

	public long position() throws IOException {
		return file.getFilePointer();
	}

	public void close() throws IOException {
		file.close();
	}
}
