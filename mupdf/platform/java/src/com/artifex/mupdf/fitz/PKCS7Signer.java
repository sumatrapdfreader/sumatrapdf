package com.artifex.mupdf.fitz;

public abstract class PKCS7Signer
{
	static {
		Context.init();
	}

	private long pointer;

	protected native void finalize();

	public void destroy() {
		finalize();
	}

	private native long newNative(PKCS7Signer signer);

	protected PKCS7Signer() {
		pointer = newNative(this);
	}

	public abstract PKCS7DesignatedName name();
	public abstract byte[] sign(FitzInputStream stm);

	// Returns a value equal to at least the number of bytes required to store the signing digest.
	// This should be based on the chosen signing certificate (and any associated auxiliary
	// certificates required)
	public abstract int maxDigest();
}
