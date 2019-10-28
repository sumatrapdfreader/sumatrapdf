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

import javax.swing.*;
import java.awt.*;
import java.awt.image.*;
import java.awt.geom.AffineTransform;

public class RegimViewer extends JPanel
{  
    private PnmImage refpnm;
    private int vw, vh;
    private Image refimg;
    private Image jpipImg;
    private double[] affine_matrix;
    private AffineTransform affine;
    
    public RegimViewer( String refname, double[] mat)
    {
	refpnm = new PnmImage( refname.replaceFirst("jp2", "pgm")); // decoding not realized
	affine_matrix = new double[6];

	affine_matrix[0] = mat[0];
	affine_matrix[1] = mat[3];
	affine_matrix[2] = mat[1];
	affine_matrix[3] = mat[4];
	affine_matrix[4] = mat[2];
	affine_matrix[5] = mat[5];
	
	affine = new AffineTransform();

	for( int i=0; i<3; i++){
	    for( int j=0; j<3; j++)
		System.out.print( mat[i*3+j] + " ");
	    System.out.println();
	}
    }
    
    public void projection( Image jpipimg, double scale)
    {
	jpipImg = jpipimg;
	refimg = refpnm.createScaleImage( scale);
	vw = refimg.getWidth(this);
	vh = refimg.getHeight(this);
	this.setSize( vw, vh);
	
	affine.setTransform( affine_matrix[0], affine_matrix[1], affine_matrix[2], affine_matrix[3], affine_matrix[4], affine_matrix[5]);
	repaint();
    }
    
    public void paint(Graphics g)
    {
	int iw, ih;
	BufferedImage bi, bi2;
	Graphics2D big, big2;
	Graphics2D g2 = (Graphics2D) g;
		
	g2.clearRect(0, 0, vw, vh);
	
	g2.setRenderingHint(RenderingHints.KEY_ANTIALIASING,
			    RenderingHints.VALUE_ANTIALIAS_ON);
	g2.setRenderingHint(RenderingHints.KEY_RENDERING,
			    RenderingHints.VALUE_RENDER_QUALITY);
	
	iw = refimg.getWidth(this);
	ih = refimg.getHeight(this);
	
	bi = new BufferedImage( iw, ih, BufferedImage.TYPE_INT_RGB);
	big = bi.createGraphics();
	big.drawImage(refimg, 0, 0, this);
	
	g2.drawImage(bi, 0, 0, this);

	bi2 = new BufferedImage( jpipImg.getWidth(this), jpipImg.getHeight(this), BufferedImage.TYPE_INT_RGB);
	big2 = bi2.createGraphics();
	big2.drawImage( jpipImg, 0, 0, this);
	
	g2.setTransform(affine);

	g2.drawImage(bi2, 0, 0, this);
    }
    
    public Dimension get_imsize()
    {
	return (new Dimension( vw, vh));
    }
}
