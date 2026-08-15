// Copyright (C) 2004-2025 Artifex Software, Inc.
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

public class ColorSpace
{
	static {
		Context.init();
	}

	private long pointer;

	protected native void finalize();

	public void destroy() {
		finalize();
	}

	private ColorSpace(long p) {
		pointer = p;
	}

	private native long newNativeColorSpace(String name, Buffer buffer);

	public ColorSpace(String name, Buffer buffer) {
		pointer = newNativeColorSpace(name, buffer);
	}

	private static native long nativeDeviceGray();
	private static native long nativeDeviceRGB();
	private static native long nativeDeviceBGR();
	private static native long nativeDeviceCMYK();

	protected static ColorSpace fromPointer(long p) {
		if (p == DeviceGray.pointer) return DeviceGray;
		if (p == DeviceRGB.pointer) return DeviceRGB;
		if (p == DeviceBGR.pointer) return DeviceBGR;
		if (p == DeviceCMYK.pointer) return DeviceCMYK;
		return new ColorSpace(p);
	}

	public static ColorSpace DeviceGray = new ColorSpace(nativeDeviceGray());
	public static ColorSpace DeviceRGB = new ColorSpace(nativeDeviceRGB());
	public static ColorSpace DeviceBGR = new ColorSpace(nativeDeviceBGR());
	public static ColorSpace DeviceCMYK = new ColorSpace(nativeDeviceCMYK());
	public native int getNumberOfComponents();
	public native String toString();

	public native boolean isGray();
	public native boolean isRGB();
	public native boolean isCMYK();
	public native boolean isIndexed();
	public native boolean isLab();
	public native boolean isDeviceN();
	public native boolean isSubtractive();

	public native int getType();

	public static final int NONE = 0;
	public static final int GRAY = 1;
	public static final int RGB = 2;
	public static final int BGR = 3;
	public static final int CMYK = 4;
	public static final int LAB = 5;
	public static final int INDEXED = 6;
	public static final int SEPARATION = 7;
}
