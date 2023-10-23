// Copyright (C) 2022 Artifex Software, Inc.
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

/**
 * Render a page from a document at given zoom and rotation.
 *
 * To build this example in a source tree:
 * make -C platform/java examples
 *
 * To render page 2 from a document (at 100% without rotation)
 * and output the result as PNM to stdout, run:
 * java -classpath build/java/debug -Djava.library.path=build/java/debug \
 *         example.Example document.pdf 2
 *
 * For 150% size with 90 degree rotation, pass two more arguments:
 * java -classpath build/java/debug -Djava.library.path=build/java/debug \
 *         example.Example document.pdf 2 150 90
 */

package example;

/* Import all MuPDF java classes. */
import com.artifex.mupdf.fitz.*;

class Example
{
	public static void main(String args[])
	{
		/* Parse arguments. */
		if (args.length < 2)
		{
			System.err.println("usage: Example input-file page-number [ zoom [ rotate] ]");
			System.err.println("\tinput-file: path of PDF, XPS, CBZ or EPUB document to open");
			System.err.println("\tPage numbering starts from one.");
			System.err.println("\tZoom level is in percent (100 percent is 72 dpi).");
			System.err.println("\tRotation is in degrees clockwise.");
			return;
		}

		String filename = args[0];
		int pageNumber = Integer.parseInt(args[1]) - 1;
		float zoom = 100;
		if (args.length >= 3)
			zoom = Float.parseFloat(args[2]);
		float rotate = 0;
		if (args.length >= 4)
			rotate = Float.parseFloat(args[3]);

		/* Open document using path. */
		Document doc;
		try {
			doc = Document.openDocument(filename);
		} catch (RuntimeException e) {
			System.err.println("Error opening document: " + e);
			return;
		}

		/* Count the number of pages. */
		int pageCount;
		try {
			pageCount = doc.countPages();
		} catch (RuntimeException e) {
			System.err.println("Error counting number of pages: " + e);
			return;
		}

		/* Check that the desired page is in range. */
		if (pageNumber < 0 || pageNumber >= pageCount)
		{
			System.err.println("Page number out of range: " + pageNumber + " (page count " + pageCount + ")");
			return;
		}

		/* Load the desired page from the document. */
		Page page;
		try {
			page = doc.loadPage(pageNumber);
		} catch (RuntimeException e) {
			System.err.println("Error loading page " + (pageNumber + 1) + ": " + e);
			return;
		}

		/* Create a transformation matrix based on
		 * zoom factor and rotation supplied by user.
		 */
		Matrix ctm = Matrix.Scale(zoom / 100);
		ctm.concat(Matrix.Rotate(rotate));

		/* Render page to an RGB pixmap without transparency. */
		Pixmap pixmap = null;
		try {
			pixmap = page.toPixmap(ctm, ColorSpace.DeviceRGB, false, true);
		} catch (RuntimeException e) {
			System.err.println("Error loading page " + (pageNumber + 1) + ": " + e);
			return;
		}

		/* Output RGB pixmap to PNM image. */
		System.out.println("P3");
		System.out.println(pixmap.getWidth() + " " + pixmap.getHeight());
		System.out.println("255");
		for (int y = 0; y < pixmap.getHeight(); y++)
		{
			for (int x = 0; x < pixmap.getWidth(); x++)
			{
				int r = ((int) pixmap.getSample(x, y, 0)) & 0xff;
				int g = ((int) pixmap.getSample(x, y, 1)) & 0xff;
				int b = ((int) pixmap.getSample(x, y, 2)) & 0xff;
				if (x == 0)
					System.out.format("%3d %3d %3d", r, g, b);
				else
					System.out.format("  %3d %3d %3d", r, g, b);
			}
			System.out.println("");
		}
	}
}
