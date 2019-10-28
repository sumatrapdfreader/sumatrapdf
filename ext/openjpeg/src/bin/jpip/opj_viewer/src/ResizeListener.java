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

import java.awt.*;
import java.awt.event.*;
import javax.swing.*;

class ResizeListener implements ComponentListener
{
    private ImageViewer iv;
    private Dimension largest;
    
    public ResizeListener( ImageViewer _iv)
    {
	iv = _iv;
	largest = iv.getSize();
    }

    public void componentHidden(ComponentEvent e) {}

    public void componentMoved(ComponentEvent e) {}

    public void componentResized(ComponentEvent e) {
	Dimension cursize = iv.getSize();
	if( largest.getWidth() < cursize.getWidth() || largest.getHeight() < cursize.getHeight()){
	    update_largest( cursize);
	    iv.enlarge();
	}
    }
    
    private void update_largest( Dimension cursize)
    {
	if( largest.getWidth() < cursize.getWidth())
	    largest.setSize( cursize.getWidth(), largest.getHeight());
	if( largest.getHeight() < cursize.getHeight())
	    largest.setSize( largest.getWidth(), cursize.getHeight());
    }

    public void componentShown(ComponentEvent e) {}
}
