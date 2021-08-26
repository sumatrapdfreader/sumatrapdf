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

	public String toString() {
		if (this == DeviceGray) return "DeviceGray";
		if (this == DeviceRGB) return "DeviceRGB";
		if (this == DeviceBGR) return "DeviceBGR";
		if (this == DeviceCMYK) return "DeviceCMYK";
		return "ColorSpace(" + getNumberOfComponents() + ")";
	}
}
