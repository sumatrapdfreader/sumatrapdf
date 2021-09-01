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

// This class handles the loading of the MuPDF shared library, together
// with the ThreadLocal magic to get the required context.
//
// The only publicly accessible method here is Context.setStoreSize, which
// sets the store size to use. This must be called before any other MuPDF
// function.
public class Context
{
	// Make sure to initialize inited before calling
	// init() from the static block below.
	private static boolean inited = false;

	static {
		init();
	}

	private static native int initNative();

	public static void init() {
		if (!inited) {
			inited = true;
			try {
				System.loadLibrary("mupdf_java");
			} catch (UnsatisfiedLinkError e) {
				try {
					System.loadLibrary("mupdf_java64");
				} catch (UnsatisfiedLinkError ee) {
					System.loadLibrary("mupdf_java32");
				}
			}
			if (initNative() < 0)
				throw new RuntimeException("cannot initialize mupdf library");
		}
	}

	// FIXME: We should support the store size being changed dynamically.
	// This requires changes within the MuPDF core.
	//public native static void setStoreSize(long newSize);

	//  empty the store
	public native static void emptyStore();

	public native static void enableICC();
	public native static void disableICC();
	public native static void setAntiAliasLevel(int level);

	public native static Version getVersion();

	public class Version {
		public String version;
		public int major;
		public int minor;
		public int patch;
	}

	public static void setLog(Log log_) {
		synchronized(lock) {
			log = log_;
		}
	}

	public interface Log
	{
		void error(String message);
		void warning(String message);
	}

	private static Log log;
	private final static Object lock = new Object();
}
