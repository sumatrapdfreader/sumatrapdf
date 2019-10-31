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
		pointer = 0;
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
