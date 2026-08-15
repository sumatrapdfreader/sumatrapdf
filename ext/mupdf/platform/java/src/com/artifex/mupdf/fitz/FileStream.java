// Copyright (C) 2004-2021 Artifex Software, Inc.
//
// This file is part of MuPDF.
//
// MuPDF is free software: you can redistribute it and/or modify it under the
// terms of the GNU Affero General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
//
// MuPDF is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more
// details.
//
// You should have received a copy of the GNU Affero General Public License
// along with MuPDF. If not, see <https://www.gnu.org/licenses/agpl-3.0.en.html>
//
// Alternative licensing terms are available from the licensor.
// For commercial licensing, see <https://www.artifex.com/> or contact
// Artifex Software, Inc., 39 Mesa Street, Suite 108A, San Francisco,
// CA 94129, USA, for further information.

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

	public void truncate() throws IOException {
		file.setLength(file.getFilePointer());
	}
}
