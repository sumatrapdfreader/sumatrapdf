/*
 * $Id$
 *
 * Copyright (c) 2002-2014, Universite catholique de Louvain (UCL), Belgium
 * Copyright (c) 2002-2014, Professor Benoit Macq
 * Copyright (c) 2010-2011, Kaori Hagihara
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS `AS IS'
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

import java.awt.event.*;

class MML implements MouseMotionListener, MouseListener
{
    public void mouseExited(MouseEvent e) {}
    public void mouseEntered(MouseEvent e) {}
    public void mouseClicked(MouseEvent e) {}
  
    private ImageViewer iv;
    private int x1, y1, x2, y2, zf, btn;
    private boolean zoomrq;
  
    public MML(ImageViewer imageviewer)
    {
	x1 = y1 = -1;
	iv = imageviewer;
	zoomrq = false;
	zf = 0;
    }
  
    private boolean isInside(int x, int y)
    {
	x -= iv.getX();
	y -= iv.getY();
	return (x >= 0) && (x < iv.getWidth())
	    && (y >= 0) && (y < iv.getHeight());
    }

    public void mousePressed(MouseEvent e)
    {
	btn = e.getButton();
	
	if( iv.hasAnnotation()){
	    if( iv.isInsideROIRect(e.getX(), e.getY())){
		iv.zoomIn();
		System.out.println("annotation click");
		return;
	    }
	}
	if (iv.isInsideRect(e.getX(), e.getY())) {
	    iv.setSelected(2);
	    iv.repaint();
	    zoomrq = true;
	} else {
	    iv.setRGeom(0, 0, 0, 0);
	    iv.setSelected(0);
	    iv.repaint();
	    x1 = y1 = -1;
	}
    }
  
    public void mouseReleased(MouseEvent e)
    {
	if(e.getButton() == 1) {
	    if (zoomrq) {
		iv.zoomIn();
		zoomrq = false;
	    }
	}
    }

    public void mouseMoved(MouseEvent e)
    {
    }
  
    public void mouseDragged(MouseEvent e)
    {
	if (btn == 1) {
	    x2 = e.getX();
	    y2 = e.getY();

	    iv.setSelected(0);
	    zoomrq = false;

	    if (isInside(x2, y2)) {
		if (x1 == -1) {
		    x1 = x2;
		    y1 = y2;
		} else {
		    iv.setRGeom(x1, y1, x2, y2);
		    iv.repaint();
		}
	    }
	}    
    }
}
