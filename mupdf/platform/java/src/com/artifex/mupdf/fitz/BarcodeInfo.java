// Copyright (C) 2025 Artifex Software, Inc.
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

public class BarcodeInfo
{
	static {
		Context.init();
	}

	public static final int BARCODE_NONE = 0;
	public static final int BARCODE_AZTEC = 1;
	public static final int BARCODE_CODABAR = 2;
	public static final int BARCODE_CODE39 = 3;
	public static final int BARCODE_CODE93 = 4;
	public static final int BARCODE_CODE128 = 5;
	public static final int BARCODE_DATABAR = 6;
	public static final int BARCODE_DATABAREXPANDED = 7;
	public static final int BARCODE_DATAMATRIX = 8;
	public static final int BARCODE_EAN8 = 9;
	public static final int BARCODE_EAN13 = 10;
	public static final int BARCODE_ITF = 11;
	public static final int BARCODE_MAXICODE = 12;
	public static final int BARCODE_PDF417 = 13;
	public static final int BARCODE_QRCODE = 14;
	public static final int BARCODE_UPCA = 15;
	public static final int BARCODE_UPCE = 16;
	public static final int BARCODE_MICROQRCODE = 17;
	public static final int BARCODE_RMQRCODE = 18;
	public static final int BARCODE_DXFILMEDGE = 19;
	public static final int BARCODE_DATABARLIMITED = 20;

	public int type;
	public String contents;

	public BarcodeInfo(int type, String contents) {
		this.type = type;
		this.contents = contents;
	}

	public native String toString();
}
