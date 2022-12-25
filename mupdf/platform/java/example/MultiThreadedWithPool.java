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
// Artifex Software, Inc., 1305 Grant Avenue - Suite 200, Novato,
// CA 94945, U.S.A., +1(415)492-9861, for further information.

/**
 * Multi-threaded rendering using a thread pool.
 *
 * First look at MultiThreaded.java and make sure you understand it.
 * The caution at the top of the file mentions that creating one thread
 * per page in a document with many pages may create performance issues.
 *
 * The code below both renders pages to pixmaps in a Callable task scheduled on
 * a thread pool provided by ExecutorService. The number of threads in the
 * thread pool is limited to four here as an example.
 *
 * To build this example in a source tree:
 * make -C platform/java examples
 *
 * To render all page from a document and output PNGs, run:
 * java -classpath build/java/debug -Djava.library.path=build/java/debug \
 *         example.MultiThreadedWithPool document.pdf
 */

package example;

/* Import classes for scheduling on a thread pool. */
import java.util.*;
import java.util.concurrent.*;

/* Import all MuPDF java classes. */
import com.artifex.mupdf.fitz.*;

class MultiThreadedWithPool
{
	public static void main(String args[])
	{
		/* Parse arguments. */
		if (args.length < 1)
		{
			System.err.println("usage: MultiThreadedWithPool input-file");
			System.err.println("\tinput-file: path of PDF, XPS, CBZ or EPUB document to open");
			return;
		}

		/* Open the document and count its pages on the main thread. */
		String filename = args[0];
		Document doc;
		int pages;
		try {
			doc = Document.openDocument(filename);
			pages = doc.countPages();
		} catch (RuntimeException ex) {
			System.err.println("Could not open input file: " + ex.getMessage());
			return;
		}

		/* Create an ExecutorService with a thread pool of 4 threads. */
		ExecutorService executor = Executors.newFixedThreadPool(4);
		try {
			/* A list holding futures for the rendered images from each page. */
			List<Future<Pixmap>> renderingFutures = new LinkedList();

			for (int i = 0; i < pages; ++i) {
				final int pageNumber = i;
				Page page =  doc.loadPage(pageNumber);
				final Rect bounds = page.getBounds();
				final DisplayList displayList = page.toDisplayList();

				/* Append this callable to the list of page rendering futures above.
				 * It may not be scheduled to run by the executor until later when
				 * asking for the resulting pixmap.
				 */
				renderingFutures.add(executor.submit(new Callable<Pixmap>() {
					public Pixmap call() {
						System.out.println(pageNumber + ": rendering display list to pixmap");

						/* Create a white destination pixmap with correct dimensions. */
						Pixmap pixmap = new Pixmap(ColorSpace.DeviceRGB, bounds);
						pixmap.clear(0xff);

						/* Run the display list through a DrawDevice which
						 * will render the requested area of the page to the
						 * given pixmap.
						 */
						DrawDevice dev = new DrawDevice(pixmap);
						displayList.run(dev, Matrix.Identity(), bounds, null);
						dev.close();

						/* Return the rendered pixmap to the future. */
						return pixmap;
					}
				}));
			}

			/* Get the resulting pixmap from each page rendering future. */
			for (int i = pages - 1; i >= 0; --i) {
				Pixmap pixmap = renderingFutures.get(i).get();
				/* Save destination pixmap to a PNG. */
				pixmap.saveAsPNG(String.format("out-%04d.png", i));
				System.out.println(i + ": pixmap saved to PNG");
			}

			System.out.println("Rendered all pages to PNG files!");
		} catch (Exception ex) {
			System.err.println("Could not render all pages: " + ex.getMessage());
		} finally {
			/* Stop all thread pool threads. */
			executor.shutdown();
		}
	}
}
