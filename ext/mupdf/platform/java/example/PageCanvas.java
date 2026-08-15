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

import com.artifex.mupdf.fitz.*;

import java.awt.*;
import java.awt.event.*;
import java.awt.image.*;

public class PageCanvas extends Canvas
{
	protected float pixelScale;
	protected BufferedImage image;
	protected Rect[] links;
	protected Quad[][] hits;

	public PageCanvas(float pixelScale) {
		this.pixelScale = pixelScale;
	}

	public void setPage(BufferedImage image, Rect[] links, Quad[][] hits) {
		this.image = image;
		this.links = links;
		this.hits = hits;
		repaint();
	}

	public Dimension getPreferredSize() {
		if (image == null)
			return new Dimension(600, 700);

		return new Dimension(image.getWidth(), image.getHeight());
	}

	public void paint(Graphics g) {
		if (image == null)
			return;

		float imageScale = 1 / pixelScale;
		final Graphics2D g2d = (Graphics2D)g.create(0, 0, image.getWidth(), image.getHeight());
		g2d.scale(imageScale, imageScale);
		g2d.drawImage(image, 0, 0, null);

		if (hits != null) {
			g2d.setColor(new Color(1, 0, 0, 0.4f));

			for (Quad[] hit : hits)
				for (Quad q : hit) {
					int[] x = new int[]{(int)q.ul_x, (int)q.ur_x, (int)q.lr_x, (int)q.ll_x };
					int[] y = new int[]{(int)q.ul_y, (int)q.ur_y, (int)q.lr_y, (int)q.ll_y };
					g2d.fillPolygon(x, y, 4);
				}
		}

		if (links != null) {
			g2d.setColor(new Color(0, 0, 1, 0.1f));
			for (Rect link : links)
				g2d.fillRect((int)link.x0, (int)link.y0, (int)(link.x1-link.x0), (int)(link.y1-link.y0));
		}

		g2d.dispose();
	}

	public Dimension getMinimumSize() { return getPreferredSize(); }
	public Dimension getMaximumSize() { return getPreferredSize(); }
	public void update(Graphics g) { paint(g); }
}
