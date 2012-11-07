package com.artifex.mupdf;

import android.graphics.Bitmap;

public class BitmapHolder {
	private Bitmap bm;

	public BitmapHolder() {
		bm = null;
	}

	public synchronized void setBm(Bitmap abm) {
		bm = abm;
	}

	public synchronized Bitmap getBm() {
		return bm;
	}
}
