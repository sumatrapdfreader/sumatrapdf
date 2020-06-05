package com.artifex.mupdf.fitz;

import java.nio.ByteBuffer;
import java.util.Map;

public abstract class PKCS7Verifier
{
	static {
		Context.init();
	}

	// Define possible values for signature verification results
	public final static int PKCS7VerifierOK                = 0;
	public final static int PKCS7VerifierNoSignature       = 1;
	public final static int PKCS7VerifierNoCertificate     = 2;
	public final static int PKCS7VerifierDigestFailure     = 3;
	public final static int PKCS7VerifierSelfSigned        = 4;
	public final static int PKCS7VerifierSelfSignedInChain = 5;
	public final static int PKCS7VerifierNotTrusted        = 6;
	public final static int PKCS7VerifierUnknown           = -1;

	static {
		Context.init();
	}

	private long pointer;

	protected native void finalize();

	public void destroy() {
		finalize();
	}

	private native long newNative(PKCS7Verifier verifier);

	protected PKCS7Verifier() {
		pointer = newNative(this);
	}

	public abstract int checkDigest(FitzInputStream stream, byte[] signature);
	public abstract int checkCertificate(byte[] signature);
}
