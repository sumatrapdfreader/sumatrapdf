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

public class OptionPanel extends JPanel implements ActionListener
{
    private JButton roibutton;
    private JButton imregbutton;
    private ImageManager imgmanager;
    private ImageViewer iv;
    private JP2XMLparser xmlparser;
    private JFrame regimwindow;
    private RegimViewer regimgviewer;
    
    public OptionPanel( ImageManager manager, ImageViewer imgviewer)
    {
	this.setLayout(new BoxLayout( this, BoxLayout.Y_AXIS));

	roibutton = new JButton("Region Of Interest");
	imregbutton = new JButton("Image Registration");

	roibutton.setAlignmentX( Component.CENTER_ALIGNMENT);
	imregbutton.setAlignmentX( Component.CENTER_ALIGNMENT);
	
	add( roibutton);
	add( imregbutton);
	roibutton.addActionListener(this);
	imregbutton.addActionListener(this);

	imgmanager = manager;
	iv = imgviewer;
	xmlparser = null;
    }

    public void actionPerformed(ActionEvent e)
    {
	if( xmlparser == null){
	    byte []xmldata = imgmanager.getXML();
	    if( xmldata != null)
		xmlparser = new JP2XMLparser( xmldata);
	}
	if( e.getSource() == roibutton){
	    if( xmlparser != null){
		JP2XMLparser.ROIparams roi[] = xmlparser.getROIparams();
		iv.annotate( roi);
	    }
	}
	if( e.getSource() == imregbutton){
	    if( xmlparser != null){
		if( regimwindow == null){
		    JP2XMLparser.IRTparams irt = xmlparser.getIRTparams();
		    
		    regimgviewer = new RegimViewer( irt.refimg, irt.mat);
		    regimgviewer.setOpaque(false);

		    regimwindow = new JFrame("Registered Image");
		    regimwindow.getContentPane().add("Center", regimgviewer);
		    regimwindow.pack();
		    regimwindow.setLocation( 500, 50);
		    regimwindow.setVisible(true);
		}
		regimgviewer.projection( iv.getImage(), (double)imgmanager.getRw()/(double)imgmanager.getOrigWidth());
		regimwindow.setSize( regimgviewer.get_imsize());
		regimwindow.show();
	    }
	}
    }
}