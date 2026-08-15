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
