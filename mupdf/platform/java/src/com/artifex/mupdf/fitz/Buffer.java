// Copyright (C) 2004-2023 Artifex Software, Inc.
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

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

public class Buffer
{
	static {
		Context.init();
	}

	private long pointer;

	protected native void finalize();

	public void destroy() {
		finalize();
	}

	private native long newNativeBuffer(int n);

	public Buffer(int n) {
		pointer = newNativeBuffer(n);
	}

	public Buffer() {
		pointer = newNativeBuffer(0);
	}

	protected Buffer(long p) {
		pointer = p;
	}

	public native int getLength();
	public native int readByte(int at);
	public native int readBytes(int at, byte[] bs);
	public native int readBytesInto(int at, byte[] bs, int off, int len);

	public native void writeByte(byte b);
	public native void writeBytes(byte[] bs);
	public native void writeBytesFrom(byte[] bs, int off, int len);
	public native void writeBuffer(Buffer buf);
	public native void writeRune(int rune);
	public native void writeLine(String line);
	public native void writeLines(String... lines);

	public native Buffer slice(int start, int end);

	public Buffer slice(int start)
	{
		return slice(start, getLength());
	}

	public Buffer slice()
	{
		return slice(0, getLength());
	}

	public native void save(String filename);

	public void readIntoStream(OutputStream stream)
	{
		try {
			byte[] data = new byte[getLength()];
			readBytes(0, data);
			stream.write(data);
		} catch (IOException e) {
			throw new RuntimeException("unable to write all bytes from buffer into stream");
		}
	}

	public void writeFromStream(InputStream stream)
	{
		try {
			boolean readAllBytes = false;
			byte[] data = null;
			while (!readAllBytes)
			{
				int availableBytes = stream.available();
				if (data == null || availableBytes > data.length)
					data = new byte[availableBytes];
				int bytesRead = stream.read(data);
				if (bytesRead >= 0)
					writeBytesFrom(data, 0, bytesRead);
				else
					readAllBytes = true;
			}
		} catch (IOException e) {
			throw new RuntimeException("unable to read all bytes from stream into buffer");
		}
	}

}
