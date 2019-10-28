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
import java.awt.image.*;
import java.io.*;
import java.util.regex.*;

public class PnmImage extends Component
{
    private byte[] data = null;
    private int width = 0;
    private int height = 0;
    private int channel = 0;
    
    public PnmImage( int c, int w, int h)
    {
	channel = c;
	width   = w;
	height  = h;
	data = new byte [ w*h*c];
    }
       
    public PnmImage( String filename)
    {
	String  str;
	Pattern pat;
	Matcher mat;
	int bytes;
	int r, offset = 0;
		
	try {
	    FileInputStream fis = new FileInputStream( new File(filename));    
	    DataInputStream is = new DataInputStream( fis);
	    
	    pat = Pattern.compile("^P([56])$");
	    mat = pat.matcher(str = is.readLine());
	    if( !mat.matches()){
		System.out.println("PNM header format error");
		return;
	    }

	    if( (mat.group(1)).compareTo("5") == 0)
		channel = 1;
	    else
		channel = 3;
	    
	    pat = Pattern.compile("^(\\d+) (\\d+)$");
	    mat = pat.matcher(str = is.readLine());
	    if( !mat.matches()){
		System.out.println("PNM header format error");
		return;
	    }
	    width  = Integer.parseInt( mat.group(1));
	    height = Integer.parseInt( mat.group(2));

	    str = is.readLine(); // 255
	    
	    bytes = width*height*channel;
	    data = new byte[bytes];
	    	    
	    while( bytes > 0){
		try {
		    r = is.read(data, offset, bytes);
		    if( r == -1){
			System.err.println("    failed to read()");
			break;
		    }
		    offset += r; 
		    bytes -= r; 
		}
		catch (IOException e) { e.printStackTrace(); }
	    }    
	    fis.close();
	} catch (IOException e) { e.printStackTrace(); }
    }

    public byte [] get_data(){	return data;}
    public int get_width() { return width;}
    public int get_height(){ return height;}
    
    public Image createROIImage( int rx, int ry, int rw, int rh)
    {
	int []pix = new int[ rw*rh];
	
	for( int i=0; i<rh; i++)
	    for( int j=0; j<rw; j++){
		pix[i*rw+j] = 0xFF << 24; // transparency
		if( channel == 1){
		    Byte lum = data[(ry+i)*width+rx+j];
		    short slum;
	  
		    if( lum < 0)
			slum = (short)(2*128+lum);
		    else
			slum = (short)lum;
	  
		    for( int c=0; c<3; c++){
			pix[i*rw+j] = pix[i*rw+j] | slum << (8*c);
		    }
		}
		else
		    for( int c=0; c<3; c++){
			Byte lum = data[ ((ry+i)*width+rx+j)*channel+(2-c)];
			short slum;
	    
			if( lum < 0)
			    slum = (short)(2*128+lum);
			else
			    slum = (short)lum;
	    
			pix[i*rw+j] = pix[i*rw+j] | slum << (8*c);
		    }
	    }

	return createImage(new MemoryImageSource( rw, rh, pix, 0, rw));
    }

    public Image createScaleImage( double scale)
    {
    	Image src = createROIImage( 0, 0, width, height);	
    	ImageFilter replicate = new ReplicateScaleFilter( (int)(width*scale), (int)(height*scale));
    	ImageProducer prod = new FilteredImageSource( src.getSource(), replicate);
	
    	return createImage(prod);
    }
}
