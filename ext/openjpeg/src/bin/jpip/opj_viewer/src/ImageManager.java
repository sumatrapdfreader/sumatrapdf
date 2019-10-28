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

import java.awt.Image;

public class ImageManager extends JPIPHttpClient
{
    private PnmImage pnmimage;
    private int origwidth;
    private int origheight;
    private ImgdecClient imgdecoder;

    public ImageManager( String uri, String host, int port)
    {
	super( uri);
	imgdecoder = new ImgdecClient( host, port);
	pnmimage = null;
	origwidth = 0;
	origheight = 0;
    }
    
    public int getOrigWidth(){
	if( origwidth == 0){
	    if( cid != null || tid != null){
		java.awt.Dimension dim = imgdecoder.query_imagesize( cid, tid);
		if( dim != null){
		    origwidth = dim.width;
		    origheight = dim.height;
		}
	    }
	    else
		System.err.println("Neither cid or tid obtained before to get Original Image Dimension");
	}
	return origwidth;
    }
    public int getOrigHeight(){ return origheight;}
    
    public Image getImage( String j2kfilename, int reqfw, int reqfh, boolean reqcnew, int reqaux, boolean reqJPP, boolean reqJPT)
    {
	System.err.println();
	
	String refcid = null;
	byte[] jpipstream;
	
	// Todo: check if the cid is for the same stream type
	if( reqcnew)
	    refcid = imgdecoder.query_cid( j2kfilename);
	
	if( refcid == null){
	    String reftid = imgdecoder.query_tid( j2kfilename);
	    if( reftid == null)
		jpipstream = super.requestViewWindow( j2kfilename, reqfw, reqfh, reqcnew, reqaux, reqJPP, reqJPT);
	    else
		jpipstream = super.requestViewWindow( j2kfilename, reftid, reqfw, reqfh, reqcnew, reqaux, reqJPP, reqJPT);
	}
	else
	    jpipstream = super.requestViewWindow( reqfw, reqfh, refcid, reqcnew, reqaux, reqJPP, reqJPT);
	
	System.err.println( "decoding to PNM image");
	if((pnmimage = imgdecoder.decode_jpipstream( jpipstream, j2kfilename, tid, cid, fw, fh))!=null){
	    System.err.println( "     done");
	    return pnmimage.createROIImage( rx, ry, rw, rh);
	}
	else{
	    System.err.println( "     failed");
	    return null;
	}
    }
    
    public Image getImage( int reqfw, int reqfh, int reqrx, int reqry, int reqrw, int reqrh)
    {
	System.err.println();
	
	byte[] jpipstream = super.requestViewWindow( reqfw, reqfh, reqrx, reqry, reqrw, reqrh);

	System.err.println( "decoding to PNM image");
	if((pnmimage = imgdecoder.decode_jpipstream( jpipstream, tid, cid, fw, fh)) != null){
	    System.err.println( "     done");
	    return pnmimage.createROIImage( rx, ry, rw, rh);
	}
	else{
	    System.err.println( "     failed");
	    return null;
	}
    }
    
    public byte[] getXML()
    {
	System.err.println();
	
	byte []xmldata = null;
	byte[] jpipstream = super.requestXML();
	
	if( jpipstream != null){
	    imgdecoder.send_JPIPstream( jpipstream);
      
	    xmldata = imgdecoder.get_XMLstream( cid);    
	}
	return xmldata;
    }

    public void closeChannel()
    {
	if( cid != null){
	    imgdecoder.destroy_cid( cid);
	    super.closeChannel();
	}
    }
}
