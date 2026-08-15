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
		int pageCount;
		try {
			doc = Document.openDocument(filename);
		} catch (RuntimeException ex) {
			System.err.println("cannot open document: " + ex.getMessage());
			return;
		}

		try {
			pageCount = doc.countPages();
		} catch (RuntimeException ex) {
			System.err.println("cannot count document pages: " + ex.getMessage());
			return;
		}

		/* Create an ExecutorService with a thread pool of 4 threads. */
		ExecutorService executor = Executors.newFixedThreadPool(4);

		/* A list holding futures for the rendered images from each page. */
		List renderingFutures = new LinkedList();

		for (int i = 0; i < pageCount; ++i) {
			final int pageNumber = i;
			try {
				Page page =  doc.loadPage(pageNumber);
				final Rect bounds = page.getBounds();
				final DisplayList displayList = page.toDisplayList();

				/* Append this callable to the list of page rendering futures above.
				 * It may not be scheduled to run by the executor until later when
				 * asking for the resulting pixmap.
				 */
				renderingFutures.add(executor.submit(new Callable<Pixmap>() {
					public Pixmap call() {
						System.out.println(pageNumber + ": creating pixmap");

						/* Create a white destination pixmap with correct dimensions. */
						Pixmap pixmap = new Pixmap(ColorSpace.DeviceRGB, bounds);
						pixmap.clear(0xff);

						System.out.println(pageNumber + ": rendering display list to pixmap");

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
			} catch (RuntimeException ex) {
				System.err.println(pageNumber + ": cannot load page, skipping render: " + ex.getMessage());
				renderingFutures.add(ex);
			}
		}

		/* Get the resulting pixmap from each page rendering future. */
		System.out.println("awaiting " + pageCount + " futures");
		for (int i = 0; i < pageCount; ++i) {
			if (renderingFutures.get(i) instanceof Exception) {
				Exception ex  = (Exception) renderingFutures.get(i);
				System.err.println(i + ": skipping save, page loading failed: " + ex.toString());
				continue;
			}

			Future<Pixmap> future = (Future<Pixmap>) renderingFutures.get(i);
			if (future == null) {
				System.err.println(i + ": skipping save, page loading failed");
				continue;
			}

			Pixmap pixmap;
			try {
				pixmap = future.get();
			} catch (InterruptedException ex) {
				System.err.println(i + ": interrupted while waiting for rendering result, skipping all remaining pages: " + ex.getMessage());
				break;
			} catch (ExecutionException ex) {
				System.err.println(i + ": skipping save, page rendering failed: " + ex.getMessage());
				continue;
			}

			/* Save destination pixmap to a PNG. */
			String pngfilename = String.format("out-%04d.png", i);
			System.out.println(i + ": saving rendered pixmap as " + pngfilename);
			pixmap.saveAsPNG(pngfilename);
		}

		/* Stop all thread pool threads. */
		executor.shutdown();

		System.out.println("finally!");
	}
}
