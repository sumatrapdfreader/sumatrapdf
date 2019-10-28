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

import org.w3c.dom.Attr;
import org.w3c.dom.Document;
import org.w3c.dom.Element;
import org.w3c.dom.NodeList;
import org.xml.sax.SAXParseException;
import org.xml.sax.ErrorHandler;
import org.apache.xerces.parsers.DOMParser;
import org.xml.sax.InputSource;
import java.io.*;
import java.lang.Integer;

public class JP2XMLparser
{
    Document document;
  
    public static class ROIparams{
	public String name = null;
	public int x = 0;
	public int y = 0;
	public int w = 0;
	public int h = 0;
    }

    public static class IRTparams{
	public String refimg = null;
	public double []mat = { 0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0};
    }

    public JP2XMLparser( byte[] buf)
    {
	try{
	    InputSource source = new InputSource( new ByteArrayInputStream( buf));
	    DOMParser  parser = new DOMParser();
	    parser.setErrorHandler(new MyHandler());
	    parser.parse( source);
	    document = parser.getDocument();
	}
	catch (Exception e) {
	    e.printStackTrace();
	}
    }
  
    public ROIparams [] getROIparams()
    {
	ROIparams roi[];
	NodeList elements = document.getElementsByTagName("roi");
	int elementCount = elements.getLength();
    
	roi = new ROIparams [elementCount];

	for( int i = 0; i < elementCount; i++) {
	    Element element = (Element)elements.item(i);
      
	    roi[i] = new ROIparams();
	    roi[i].name = element.getAttribute( "name");
	    roi[i].x = Integer.parseInt( element.getAttribute( "x")) ;
	    roi[i].y = Integer.parseInt( element.getAttribute( "y")) ;
	    roi[i].w = Integer.parseInt( element.getAttribute( "w")) ;
	    roi[i].h = Integer.parseInt( element.getAttribute( "h")) ;
	}
	return roi;
    }

    public IRTparams getIRTparams()
    {
	IRTparams irt = new IRTparams();
	NodeList elements = document.getElementsByTagName("irt");
	int elementCount = elements.getLength();
	
	Element element = (Element)elements.item(0);
	irt.refimg = element.getAttribute( "refimg");
	for( int i=1; i<=9; i++)
	    irt.mat[i-1] = Double.parseDouble( element.getAttribute("m" + i));
	
	return irt;
    }
}

class MyHandler implements ErrorHandler {
    public void warning(SAXParseException e) {
	System.out.println("Warning: line" + e.getLineNumber());
	System.out.println(e.getMessage());
    }
    public void error(SAXParseException e) {
	System.out.println("Error: line" + e.getLineNumber());
	System.out.println(e.getMessage());
    }
    public void fatalError(SAXParseException e) {
	System.out.println("Critical error: line" + e.getLineNumber());
	System.out.println(e.getMessage());
    }
}