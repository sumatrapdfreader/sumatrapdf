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
// Artifex Software, Inc., 1305 Grant Avenue - Suite 200, Novato,
// CA 94945, U.S.A., +1(415)492-9861, for further information.

package com.artifex.mupdf.fitz;

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

	public native void save(String filename);
}
