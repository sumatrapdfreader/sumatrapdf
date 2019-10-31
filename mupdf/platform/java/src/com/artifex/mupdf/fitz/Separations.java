package com.artifex.mupdf.fitz;

public class Separations
{
	static {
		Context.init();
	}

	private long pointer;

	public static final int SEPARATION_COMPOSITE = 0;
	public static final int SEPARATION_SPOT = 1;
	public static final int SEPARATION_DISABLED = 2;

	protected native void finalize();

	public void destroy() {
		finalize();
		pointer = 0;
	}

	protected Separations(long p) {
		pointer = p;
	}

	public native int getNumberOfSeparations();
	public native Separation getSeparation(int separation);

	public native boolean areSeparationsControllable();
	public native int getSeparationBehavior(int separation);
	public native void  setSeparationBehavior(int separation, int behavior);
}
