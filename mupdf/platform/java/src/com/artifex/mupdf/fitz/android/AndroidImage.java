package com.artifex.mupdf.fitz.android;

import android.graphics.Bitmap;

import com.artifex.mupdf.fitz.Context;
import com.artifex.mupdf.fitz.Image;

public final class AndroidImage extends Image
{
	static {
		Context.init();
	}

	private native long newAndroidImageFromBitmap(Bitmap bitmap, long mask);

	public AndroidImage(Bitmap bitmap, AndroidImage mask)
	{
		super(0);
		pointer = newAndroidImageFromBitmap(bitmap, mask.pointer);
	}
}
