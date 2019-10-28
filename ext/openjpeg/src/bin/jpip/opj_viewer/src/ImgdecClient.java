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

import java.io.*;
import java.net.*;

public class ImgdecClient{
    
    private String hostname;
    private int portNo;

    public ImgdecClient( String host, int port)
    {
	hostname = host;
	portNo = port;
    }

    public PnmImage decode_jpipstream( byte[] jpipstream, String tid, String cid, int fw, int fh)
    {
	if( jpipstream != null)
	    send_JPIPstream( jpipstream);
	return get_PNMstream( cid, tid, fw, fh);
    }

    public PnmImage decode_jpipstream( byte[] jpipstream, String j2kfilename, String tid, String cid, int fw, int fh)
    {
	send_JPIPstream( jpipstream, j2kfilename, tid, cid);
	return get_PNMstream( cid, tid, fw, fh);
    }
    
    public void send_JPIPstream( byte[] jpipstream)
    {
	try{
	    Socket imgdecSocket = new Socket( hostname, portNo);
	    DataOutputStream os = new DataOutputStream( imgdecSocket.getOutputStream());
	    DataInputStream  is = new DataInputStream( imgdecSocket.getInputStream());
      
	    System.err.println("Sending " + jpipstream.length + "Data Bytes to decodingServer");
	    
	    os.writeBytes("JPIP-stream\n");
	    os.writeBytes("version 1.2\n");
	    os.writeBytes( jpipstream.length + "\n"); 
	    os.write( jpipstream, 0, jpipstream.length);
      
	    byte signal = is.readByte();
      
	    if( signal == 0)
		System.err.println("    failed");
	} catch (UnknownHostException e) {
	    System.err.println("Trying to connect to unknown host: " + e);
	} catch (IOException e) {
	    System.err.println("IOException: " + e);
	}
    }

    public void send_JPIPstream( byte[] jpipstream, String j2kfilename, String tid, String cid)
    {
	try{
	    Socket imgdecSocket = new Socket( hostname, portNo);
	    DataOutputStream os = new DataOutputStream( imgdecSocket.getOutputStream());
	    DataInputStream  is = new DataInputStream( imgdecSocket.getInputStream());
	    int length = 0;
	    
	    if( jpipstream != null)
		length = jpipstream.length;
	    
	    System.err.println("Sending " + length + "Data Bytes to decodingServer");
      
	    os.writeBytes("JPIP-stream\n");
	    os.writeBytes("version 1.2\n");
	    os.writeBytes( j2kfilename + "\n");
	    if( tid == null)
		os.writeBytes( "0\n");
	    else
		os.writeBytes( tid + "\n");
	    if( cid == null)
		os.writeBytes( "0\n");
	    else
		os.writeBytes( cid + "\n");
	    os.writeBytes( length + "\n");
	    os.write( jpipstream, 0, length);
	    
	    byte signal = is.readByte();
      
	    if( signal == 0)
		System.err.println("    failed");
	} catch (UnknownHostException e) {
	    System.err.println("Trying to connect to unknown host: " + e);
	} catch (IOException e) {
	    System.err.println("IOException: " + e);
	}
    }
    
    public PnmImage get_PNMstream( String cid, String tid, int fw, int fh)
    {
	PnmImage pnmstream = null;
	
	try {
	    Socket imgdecSocket = new Socket( hostname, portNo);
	    DataOutputStream os = new DataOutputStream( imgdecSocket.getOutputStream());
	    DataInputStream is = new DataInputStream( imgdecSocket.getInputStream());
	    byte []header = new byte[7];
	    
	    os.writeBytes("PNM request\n");
	    if( cid != null)
		os.writeBytes( cid + "\n");
	    else
		if( tid != null)
		    os.writeBytes( tid + "\n");
		else
		    os.writeBytes( "0\n");
	    os.writeBytes( fw + "\n");
	    os.writeBytes( fh + "\n");

	    read_stream( is, header, 7);
	    
	    if( header[0] == 80){
		// P5: gray, P6: color  
		byte magicknum = header[1];
		if( magicknum == 5 || magicknum == 6){
		    int c = magicknum==6 ? 3: 1;
		    int w = (header[2]&0xff)<<8 | (header[3]&0xff);
		    int h = (header[4]&0xff)<<8 | (header[5]&0xff);
		    int maxval = header[6]&0xff;
		    int length = w*h*c;
		    
		    if( maxval == 255 && length != 0){
			pnmstream = new PnmImage( c, w, h);
			read_stream( is, pnmstream.get_data(), length);
		    }
		    else
			System.err.println("Error in get_PNMstream(), only 255 is accepted");
		}
		else
		    System.err.println("Error in get_PNMstream(), wrong magick number" + header[1]);
	    }
	    else
		System.err.println("Error in get_PNMstream(), Not starting with P");
	    
	    os.close();
	    is.close();
	    imgdecSocket.close();
	} catch (UnknownHostException e) {
	    System.err.println("Trying to connect to unknown host: " + e);
	} catch (IOException e) {
	    System.err.println("IOException: " + e);
	}
	return pnmstream;
    }

    public byte [] get_XMLstream( String cid)
    {
	byte []xmldata = null;

	try{
	    Socket imgdecSocket = new Socket( hostname, portNo);
	    DataOutputStream os = new DataOutputStream( imgdecSocket.getOutputStream());
	    DataInputStream is = new DataInputStream( imgdecSocket.getInputStream());
	    byte []header = new byte[5];
	    
	    os.writeBytes("XML request\n");
	    os.writeBytes( cid + "\n");
      
	    read_stream( is, header, 5);
	    
	    if( header[0] == 88 && header[1] == 77 && header[2] == 76){
		int length = (header[3]&0xff)<<8 | (header[4]&0xff);
	
		xmldata = new byte[ length];
		read_stream( is, xmldata, length);
	    }
	    else
		System.err.println("Error in get_XMLstream(), not starting with XML");
	} catch (UnknownHostException e) {
	    System.err.println("Trying to connect to unknown host: " + e);
	} catch (IOException e) {
	    System.err.println("IOException: " + e);
	}
	return xmldata;
    }

    public String query_cid( String j2kfilename)
    {
	int []retmsglabel = new int[3];
	retmsglabel[0] = 67;
	retmsglabel[1] = 73;
	retmsglabel[2] = 68;

	return query_id( "CID request", j2kfilename, retmsglabel);
    }

    public String query_tid( String j2kfilename)
    {
	int []retmsglabel = new int[3];
	retmsglabel[0] = 84;
	retmsglabel[1] = 73;
	retmsglabel[2] = 68;

	return query_id( "TID request", j2kfilename, retmsglabel);
    }

    public String query_id( String reqmsghead, String j2kfilename, int[] retmsglabel)
    {
	String id = null;
	
	try{
	    Socket imgdecSocket = new Socket( hostname, portNo);
	    DataOutputStream os = new DataOutputStream( imgdecSocket.getOutputStream());
	    DataInputStream is = new DataInputStream( imgdecSocket.getInputStream());
	    byte []header = new byte[4];

	    os.writeBytes( reqmsghead + "\n");
	    os.writeBytes( j2kfilename + "\n");

	    read_stream( is, header, 4);
	    
	    if( header[0] == retmsglabel[0] && header[1] == retmsglabel[1] && header[2] == retmsglabel[2]){
		int length = header[3]&0xff;

		if( length > 0){
		
		    byte []iddata = new byte[ length];
		    read_stream( is, iddata, length);
		    id = new String( iddata);
		}
	    }
	    else
		System.err.println("Error in query_id("+ reqmsghead + "), wrong to start with " + header);
	}
	catch (UnknownHostException e) {
	    System.err.println("Trying to connect to unknown host: " + e);
	} catch (IOException e) {
	    System.err.println("IOException: " + e);
	}

	return id;	
    }

    public java.awt.Dimension query_imagesize( String cid, String tid)
    {
	java.awt.Dimension dim = null;

	try{
	    Socket imgdecSocket = new Socket( hostname, portNo);
	    DataOutputStream os = new DataOutputStream( imgdecSocket.getOutputStream());
	    DataInputStream is = new DataInputStream( imgdecSocket.getInputStream());
	    byte []header = new byte[3];

	    os.writeBytes( "SIZ request\n");
	    if( tid == null)
		os.writeBytes( "0\n");
	    else
		os.writeBytes( tid + "\n");
	    if( cid == null)
		os.writeBytes( "0\n");
	    else
		os.writeBytes( cid + "\n");

	    read_stream( is, header, 3);
	    
	    if( header[0] == 83 && header[1] == 73 && header[2] == 90){
		
		byte []data = new byte[ 3];
		read_stream( is, data, 3);
		int w = (data[0]&0xff)<<16 | (data[1]&0xff)<<8 | (data[2]&0xff);
		read_stream( is, data, 3);
		int h = (data[0]&0xff)<<16 | (data[1]&0xff)<<8 | (data[2]&0xff);
		dim = new java.awt.Dimension( w, h);
	    }
	    else
		System.err.println("Error in query_imagesize("+ cid + ", " + tid + "), wrong to start with " + header);
	}
	catch (UnknownHostException e) {
	    System.err.println("Trying to connect to unknown host: " + e);
	} catch (IOException e) {
	    System.err.println("IOException: " + e);
	}

	return dim;
    }
  
    private static void read_stream( DataInputStream is, byte []stream, int length)
    {
	int remlen = length;
	int off = 0;

	try{
	    while( remlen > 0){
		int redlen = is.read( stream, off, remlen);
		
		if( redlen == -1){
		    System.err.println("    failed to read_stream()");
		    break;
		}
		off += redlen;
		remlen -= redlen;
	    }
	} catch (IOException e) {
	    System.err.println("IOException: " + e);
	}
    }

    public void destroy_cid( String cid)
    {
	try{
	    Socket imgdecSocket = new Socket( hostname, portNo);
	    DataOutputStream os = new DataOutputStream( imgdecSocket.getOutputStream());
	    DataInputStream  is = new DataInputStream( imgdecSocket.getInputStream());
	    
	    os.writeBytes("CID destroy\n");
	    os.writeBytes( cid + "\n");
	    
	    byte signal = is.readByte();
      
	    if( signal == 0)
		System.err.println("    failed");
	} catch (UnknownHostException e) {
	    System.err.println("Trying to connect to unknown host: " + e);
	} catch (IOException e) {
	    System.err.println("IOException: " + e);
	}
    }
}
