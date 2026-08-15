// Copyright (C) 2004-2022 Artifex Software, Inc.
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

package example;

import java.awt.EventQueue;
import java.util.concurrent.LinkedBlockingQueue;

public class Worker implements Runnable
{
	public static class Task implements Runnable {
		public void work() {} /* The 'work' method will be executed on the background thread. */
		public void run() {} /* The 'run' method will be executed on the UI thread if work() did not throw any exception. */
		public void exception(final Throwable t) {} /* The 'exception' method will be executed on the UI thread if work() throws an exception. */
	}

	protected EventQueue eventQueue;
	protected LinkedBlockingQueue<Task> queue;
	protected boolean alive;
	protected Thread thread;

	public Worker(EventQueue eventQueue) {
		this.eventQueue = eventQueue;
		queue = new LinkedBlockingQueue<Task>();
		thread = new Thread(this);
	}

	public void start() {
		alive = true;
		thread.start();
	}

	public void stop() {
		alive = false;
		thread.interrupt();
	}

	public void add(Task task) {
		try {
			queue.put(task);
		} catch (InterruptedException x) {
			return;
		}
	}

	public void run() {
		while (alive) {
			final Task task;
			try {
				task = queue.take();
			} catch (InterruptedException x) {
				break;
			}
			try {
				task.work();
				eventQueue.invokeLater(task);
			} catch (final Throwable t) {
				eventQueue.invokeLater(new Runnable() {
					public void run() {
						task.exception(t);
					}
				});
			}
		}
	}
}
