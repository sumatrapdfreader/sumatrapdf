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
