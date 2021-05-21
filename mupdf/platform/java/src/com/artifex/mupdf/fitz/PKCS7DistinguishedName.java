package com.artifex.mupdf.fitz;

// PKCS7DistinguishedName provides a friendly representation of the
// main descriptive fields of a PKCS7 encoded certificate used
// to sign a document
public class PKCS7DistinguishedName
{
	public String cn;       // common name
	public String o;        // organization
	public String ou;       // organizational unit
	public String email;    // email address of signer
	public String c;        // country
}
