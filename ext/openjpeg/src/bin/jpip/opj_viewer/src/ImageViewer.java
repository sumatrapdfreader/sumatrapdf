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
import java.awt.image.*;
import java.awt.geom.*;
import java.net.URL;
import javax.swing.border.*;
import java.util.*;
import java.io.*;

public class ImageViewer extends JPanel
{  
    private ImageManager imgmanager;
    private int vw, vh;
    private int iw, ih;
    private int selected = 0;
    private Image img;
  
    private String cmdline = new String();
    private boolean fullRefresh = false;
    private Point offset = new Point(0,0);
    private Rectangle rect = new Rectangle();
    private Rectangle roirect[] = null;
    private String roiname[] = null;
      
    public ImageViewer( String j2kfilename, ImageManager manager, boolean session, boolean jppstream, int aux)
    {
	String str;
	MML myMML;
	
	this.setSize( 170, 170);
	Dimension asz = this.getSize();
    
	vw = asz.width;
	vh = asz.height;
    
	setBackground(Color.black);
	myMML = new MML(this);

	imgmanager = manager;

	img = imgmanager.getImage( j2kfilename, vw, vh, session, aux, jppstream, !jppstream);
	
	addMouseListener(myMML);
	addMouseMotionListener(myMML);
	addComponentListener( new ResizeListener(this));
    }

    public Image getImage()
    {
	return img;
    }
    
    public void zoomIn()
    {
	roirect = null;
	roiname = null;

	double scalex = (double)vw/(double)rect.width;
	double scaley = (double)vh/(double)rect.height;
    
	int fw = (int)(imgmanager.getFw()*scalex);
	int fh = (int)(imgmanager.getFh()*scaley);
	int rx = (int)((imgmanager.getRx()+rect.x)*scalex);
	int ry = (int)((imgmanager.getRy()+rect.y)*scaley);

	img = imgmanager.getImage( fw, fh, rx, ry, vw, vh);
  
	rect.x = rect.y = rect.width = rect.height = 0;
        
	selected = 0;
	fullRefresh = true;
	repaint();
    }

    public void enlarge()
    {
	roirect = null;
	roiname = null;
	
	Dimension asz = this.getSize();
    
	vw = asz.width;
	vh = asz.height;

	double scalex = vw/(double)imgmanager.getRw();
	double scaley = vh/(double)imgmanager.getRh();

	int fw = (int)(imgmanager.getFw()*scalex);
	int fh = (int)(imgmanager.getFh()*scaley);
	int rx = (int)(imgmanager.getRx()*scalex);
	int ry = (int)(imgmanager.getRy()*scaley);
	
	img = imgmanager.getImage( fw, fh, rx, ry, vw, vh);

	fullRefresh = true;
	repaint();
    }

    public void setSelected(int state)
    {
	roirect = null;
	roiname = null;

	if (state != selected) {
	    
	    selected = state;
	    repaint();
	}
    }
  
    public boolean isInsideRect(int x, int y)
    {
	return rect.contains(x - offset.x, y - offset.y);
    }

    public void setRGeom(int x1, int y1, int x2, int y2)
    {
	rect.x = Math.min(x1,x2) - offset.x;
	rect.y = Math.min(y1,y2) - offset.y;
	rect.width = Math.abs(x2-x1);
	rect.height = Math.abs(y2-y1);
    }
    
    // public void annotate( JP2XMLparser.ROIparams roi[])
    // {
    // 	int numofroi = roi.length;

    // 	roirect = new Rectangle [numofroi];
    // 	roiname = new String [numofroi];
	
    // 	double scale_x = imgmanager.getFw()/(double)imgmanager.getOrigWidth();
    // 	double scale_y = imgmanager.getFh()/(double)imgmanager.getOrigHeight();
    // 	int rx = imgmanager.getRx();
    // 	int ry = imgmanager.getRy();
    // 	int rw = imgmanager.getRw();
    // 	int rh = imgmanager.getRh();

    // 	for( int i=0; i<numofroi ; i++){
    // 	    int x = (int)(roi[i].x*scale_x) - rx;
    // 	    int y = (int)(roi[i].y*scale_y) - ry;
    // 	    int w = (int)(roi[i].w*scale_x);
    // 	    int h = (int)(roi[i].h*scale_y);
    // 	    if( 0<=x && 0<=y && x+w<=rw && y+h<=rh){ // can be optimized
    // 		roirect[i] = new Rectangle( x, y, w, h);
    // 		roiname[i] = new String( roi[i].name);
    // 	    }
    // 	    else{
    // 		roirect[i] = null;
    // 		roiname[i] = null;
    // 	    }
    // 	}
    // 	repaint();
    // }
    
    public boolean hasAnnotation()
    {
	if( roirect == null)
	    return false;
	else
	    return true;
    }
    
    public boolean isInsideROIRect(int x, int y)
    {
	for( int i=0; i<roirect.length; i++)
	    if( roirect[i] != null)
		if( roirect[i].contains(x - offset.x, y - offset.y)){
		    rect = roirect[i];
		    return true;
		}
	return false;
    }

    public void paint(Graphics g)
    {
	BufferedImage bi;
	Graphics2D big;
	Graphics2D g2 = (Graphics2D) g;

	if (fullRefresh) {
	    g2.clearRect(0, 0, vw, vh);
	    fullRefresh = false;
	}
	g2.setRenderingHint(RenderingHints.KEY_ANTIALIASING,
			    RenderingHints.VALUE_ANTIALIAS_ON);
	g2.setRenderingHint(RenderingHints.KEY_RENDERING,
			    RenderingHints.VALUE_RENDER_QUALITY);

	offset.x = 0;
	offset.y = 0;

	iw = img.getWidth(this);
	ih = img.getHeight(this);
    
	bi = new BufferedImage( iw, ih, BufferedImage.TYPE_INT_RGB);
	big = bi.createGraphics();
    
	big.drawImage(img, 0, 0, this);
	big.setPaint(Color.red);
	if ((rect.width > 0) && (rect.height > 0))
	    big.draw(rect);

	if( roirect != null){
	    for( int i=0; i<roirect.length; i++)
		if( roirect[i] != null){
		    big.draw( roirect[i]);
		    big.drawString( roiname[i], roirect[i].x+3, roirect[i].y+roirect[i].height*2/3);
		}
	}
	if (selected == 1)
	    shadeExt(big, 0, 0, 0, 64);
	else if (selected == 2) {
	    shadeExt(big, 0, 0, 0, 255);
	    selected = 1;
	}
	g2.drawImage(bi, offset.x, offset.y, this);
    }

    private void shadeRect(Graphics2D g2, int r, int g, int b, int a)
    {
	g2.setPaint(new Color(r, g, b, a));
	g2.fillRect(rect.x + 1, rect.y + 1, rect.width - 1, rect.height - 1);
    }
  
    private void shadeExt(Graphics2D g2, int r, int g, int b, int a)
    {
	g2.setPaint(new Color(r, g, b, a));
	g2.fillRect(0, 0, iw, rect.y); /* _N_ */
	g2.fillRect(rect.x + rect.width + 1, rect.y,
		    iw - rect.x - rect.width - 1, rect.height + 1); /* E */
	g2.fillRect(0, rect.y, rect.x, rect.height + 1); /* W */
	g2.fillRect(0, rect.y + rect.height + 1,
		    iw, ih - rect.y - rect.height - 1); /* _S_ */
    }
}
