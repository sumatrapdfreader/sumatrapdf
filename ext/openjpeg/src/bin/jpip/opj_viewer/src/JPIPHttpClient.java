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

import java.net.*;
import java.io.*;
import java.util.*;


public class JPIPHttpClient 
{    
    private String comURL;
    protected int fw, fh;
    protected int rx, ry;
    protected int rw, rh;
    protected String cid;
    protected String tid;
    private boolean JPTstream;
    private boolean JPPstream;
    private boolean aux;
    private boolean tcp; // true: tcp, false: udp
    private int port;
    
    public JPIPHttpClient( String URI)
    {
	comURL = URI + "?";
	fw = fh = -1;
	rx = ry = -1;
	rw = rh = -1;
	cid = null;
	tid = null;
	JPTstream = JPPstream = aux = false;
	port = 0;
    }

    public int getFw(){ return fw;}
    public int getFh(){ return fh;}
    public int getRx(){ return rx;}
    public int getRy(){ return ry;}
    public int getRw(){ return rw;}
    public int getRh(){ return rh;}
    
    public byte[] requestViewWindow( int reqfw, int reqfh)
    {
	if( cid != null)
	    return requestViewWindow( reqfw, reqfh, cid);
	else
	    return null;
    }
  
    public byte[] requestViewWindow( int reqfw, int reqfh, int reqrx, int reqry, int reqrw, int reqrh)
    {
	if( cid != null)
	    return requestViewWindow( reqfw, reqfh, reqrx, reqry, reqrw, reqrh, cid);
	else
	    if( tid != null)
		return requestViewWindow( null, tid, reqfw, reqfh, reqrx, reqry, reqrw, reqrh, null, false, 0, false, false);
	    else
		return null;
    }

    public byte[] requestViewWindow( int reqfw, int reqfh, String reqcid)
    {
	return requestViewWindow( null, null, reqfw, reqfh, -1, -1, -1, -1, reqcid, false, 0, false, false);
    }

    public byte[] requestViewWindow( int reqfw, int reqfh, int reqrx, int reqry, int reqrw, int reqrh, String reqcid)
    {
	return requestViewWindow( null, null, reqfw, reqfh, reqrx, reqry, reqrw, reqrh, reqcid, false, 0, false, false);
    }

    public byte[] requestViewWindow( String target, int reqfw, int reqfh)
    {
	return requestViewWindow( target, null, reqfw, reqfh, -1, -1, -1, -1, null, false, 0, false, false);
    }
    
    public byte[] requestViewWindow( String target, int reqfw, int reqfh, boolean reqcnew, int reqaux, boolean reqJPP, boolean reqJPT)
    {
	if( cid == null) // 1 channel allocation only
	    return requestViewWindow( target, null, reqfw, reqfh, -1, -1, -1, -1, null, reqcnew, reqaux, reqJPP, reqJPT);
	else
	    return null;
    }

    public byte[] requestViewWindow( String target, String reqtid, int reqfw, int reqfh, boolean reqcnew, int reqaux, boolean reqJPP, boolean reqJPT)
    {
	if( cid == null) // 1 channel allocation only
	    return requestViewWindow( target, reqtid, reqfw, reqfh, -1, -1, -1, -1, null, reqcnew, reqaux, reqJPP, reqJPT);
	else
	    return null;
    }
    
    public byte[] requestViewWindow( String target, int reqfw, int reqfh, int reqrx, int reqry, int reqrw, int reqrh)
    {
	return requestViewWindow( target, null, reqfw, reqfh, reqrx, reqry, reqrw, reqrh, null, false, 0, false, false);
    }
 
    public byte[] requestViewWindow( int reqfw, int reqfh, String reqcid, boolean reqcnew, int reqaux, boolean reqJPP, boolean reqJPT)
    {
	return requestViewWindow( null, null, reqfw, reqfh, -1, -1, -1, -1, reqcid, reqcnew, reqaux, reqJPP, reqJPT);
    }
    
    public byte[] requestViewWindow( String target,
				     String reqtid,
				     int reqfw, int reqfh, 
				     int reqrx, int reqry, 
				     int reqrw, int reqrh, 
				     String reqcid, boolean reqcnew, int reqaux, boolean reqJPP, boolean reqJPT)
    {
	if( reqtid != null)
	    tid = reqtid;

	String urlstring = const_urlstring( target, reqtid, reqfw, reqfh, reqrx, reqry, reqrw, reqrh, reqcid, reqcnew, reqaux, reqJPP, reqJPT);
	return GETrequest( urlstring);
    }
    
    public byte[] requestXML()
    {
	String urlstring = comURL;
	
	if( cid == null)
	    return null;
	
	urlstring = urlstring.concat( "cid=" + cid);
	urlstring = urlstring.concat( "&metareq=[xml_]");
    
	return GETrequest( urlstring);
    }
  
    private byte[] GETrequest( String urlstring)
    {
	URL url = null;
	HttpURLConnection urlconn = null;
	byte[] jpipstream = null;
    
	try{
	    url = new URL( urlstring);
        
	    System.err.println("Requesting: " + url);
      
	    urlconn = (HttpURLConnection)url.openConnection();
	    urlconn.setRequestMethod("GET");
	    urlconn.setInstanceFollowRedirects(false);
	    urlconn.connect();

	    set_responseheader( urlconn);
	    
	    if( !aux){
		jpipstream = receive_httpchunk( urlconn);
		urlconn.disconnect();
	    }
	    else{
		urlconn.disconnect();
		jpipstream = receive_tcpaux( comURL.substring( 7, comURL.indexOf('/', 7)), port, cid);
	    }
	}
	catch ( MalformedURLException e){
	    e.printStackTrace();
	}
	catch ( ProtocolException e){
	    e.printStackTrace();
	}    
	catch( ClassCastException  e){
	    e.printStackTrace();
	}
	catch( NullPointerException e){
	    e.printStackTrace();
	}  
	catch( UnknownServiceException e){
	    e.printStackTrace();
	}
	catch ( IOException e){
	    e.printStackTrace();
	}
	
	return jpipstream;
    }

    private void set_responseheader( HttpURLConnection urlconn)
    {
	Map<String,java.util.List<String>> headers = urlconn.getHeaderFields();
	java.util.List<String> hvaluelist;
	String hvalueline;
	
	String status = headers.get(null).get(0);
	    
	System.err.println( status);
	if( !status.contains("OK"))
	    System.err.println( headers.get("Reason"));
	    
	if(( hvaluelist = headers.get("Content-type")) == null)
	    hvaluelist = headers.get("Content-Type");
	hvalueline = hvaluelist.get(0);
	System.err.println( hvalueline);

	if( hvalueline.endsWith("jpt-stream"))
	    JPTstream = true;
	else if( hvalueline.endsWith("jpp-stream"))
	    JPPstream = true;
	    
	if(( hvaluelist = headers.get("JPIP-fsiz")) != null){
	    hvalueline = hvaluelist.get(0);
	    fw = Integer.valueOf( hvalueline.substring( 0, hvalueline.indexOf(','))).intValue();
	    fh = Integer.valueOf( hvalueline.substring( hvalueline.indexOf(',')+1 )).intValue();
	
	    System.err.println("fw,fh: " + fw + "," + fh);
	}
      
	if(( hvaluelist = headers.get("JPIP-roff")) != null){
	    hvalueline = hvaluelist.get(0);
	    rx = Integer.valueOf( hvalueline.substring( 0, hvalueline.indexOf(','))).intValue();
	    ry = Integer.valueOf( hvalueline.substring( hvalueline.indexOf(',')+1 )).intValue();
	    System.err.println("rx,ry: " + rx + "," + ry);
	}
    
	if(( hvaluelist = headers.get("JPIP-rsiz")) != null){
	    hvalueline = hvaluelist.get(0);
	    rw = Integer.valueOf( hvalueline.substring( 0, hvalueline.indexOf(','))).intValue();
	    rh = Integer.valueOf( hvalueline.substring( hvalueline.indexOf(',')+1 )).intValue();
	    System.err.println("rw,rh: " + rw + "," + rh);
	}
	    
	if(( hvaluelist = headers.get("JPIP-cnew")) != null){
	    hvalueline = hvaluelist.get(0);
	    cid = hvalueline.substring( hvalueline.indexOf('=')+1, hvalueline.indexOf(','));
		
	    int idxOfcid = hvalueline.indexOf("transport")+10;
	    int idxOfcid2 = hvalueline.indexOf(',', idxOfcid);
	    String transport;
	    if( idxOfcid2 != -1)
		transport = hvalueline.substring( idxOfcid, idxOfcid2);
	    else
		transport = hvalueline.substring( idxOfcid);

	    if( transport.matches("http-tcp")){
		aux = true;
		tcp = true;
	    }
	    else if( transport.matches("http-udp")){
		aux = true;
		tcp = false;
	    }
	    else
		aux = false;
		
	    if( aux){
		idxOfcid = hvalueline.indexOf("auxport")+8;
		port = Integer.valueOf( hvalueline.substring( idxOfcid)).intValue();
		System.err.println("cid: " + cid + ", transport: " + transport + ", auxport: " + port);
	    }
	    else		
		System.err.println("cid: " + cid + ", transport: " + transport);
	}

	if(( hvaluelist = headers.get("JPIP-tid")) != null){
	    hvalueline = hvaluelist.get(0);
	    tid = hvalueline.substring( hvalueline.indexOf('=')+1);
	    System.err.println("tid: " + tid);
	}
    }

    private static byte[] receive_httpchunk( HttpURLConnection urlconn)
    {	
	byte[] chunk = null;
	InputStream input;

	try{
	    input = urlconn.getInputStream();
		
	    if( input.available() > 0){    
		ByteArrayOutputStream tmpstream = new ByteArrayOutputStream();
		byte[] buf = new byte[ 1024];
		int redlen, buflen;

		System.err.println("reading jpipstream...");
	    
		do{
		    redlen = input.read( buf);
		
		    if( redlen == -1)
			break;
		    tmpstream.write( buf, 0, redlen);
		}while( redlen > 0);
      
		buflen = tmpstream.size();
		chunk = tmpstream.toByteArray();
            
		buf = null;
		tmpstream = null;

		System.err.println("jpiplen: " + buflen);
		System.err.println("    succeeded");
	    }
	    else{
		System.err.println("No new jpipstream");
	    }
	    input.close();
	}
	catch ( IOException e){
	    e.printStackTrace();
	}

	return chunk;
    }

    private static byte[] receive_tcpaux( String host, int port, String cid)
    {	
	Socket jpipsocket;
	DataOutputStream os;
	DataInputStream  is;
	byte []auxheader;
	byte []chunkbody = null;
	byte []stream = null;
	int chunkbodylen, streamlen, headlen = 8;
	ByteArrayOutputStream tmpstream;

	try{
	    jpipsocket = new Socket( host, port);
	    os = new DataOutputStream( jpipsocket.getOutputStream());
	    is = new DataInputStream( jpipsocket.getInputStream());
	    auxheader = new byte[headlen];
	    tmpstream = new ByteArrayOutputStream();
	
	    os.writeBytes( cid + "\r\n");

	    do{
		read_stream( is, auxheader, headlen);
            
		chunkbodylen = ((auxheader[0]&0xff)<<8 | (auxheader[1]&0xff)) - headlen;
		
		chunkbody = new byte [ chunkbodylen];
		read_stream( is, chunkbody, chunkbodylen);
		tmpstream.write( chunkbody, 0, chunkbodylen);

		os.write( auxheader, 0, headlen);
	    }while( !(chunkbody[chunkbodylen-3]==0x00 && ( chunkbody[chunkbodylen-2]==0x01 || chunkbody[chunkbodylen-2]== 0x02)));
	
	    streamlen = tmpstream.size();
	    stream = tmpstream.toByteArray();
	    
	    System.err.println("jpiplen: " + streamlen);
	    System.err.println("    succeeded");
	
	    chunkbody = null;
	    tmpstream = null;
	    
	    os.close();
	    is.close();
	    
	    jpipsocket.close();
	}
	catch ( IOException e){
	    e.printStackTrace();
	}

	return stream;
    }
    
    private static void read_stream( InputStream is, byte []stream, int length)
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
  
    private String const_urlstring( String target,
				    String reqtid,
				    int reqfw, int reqfh, 
				    int reqrx, int reqry, 
				    int reqrw, int reqrh, 
				    String reqcid, boolean reqcnew, int reqaux, boolean reqJPP, boolean reqJPT)
    {
	String urlstring = comURL;

	// C.7.3 Image Return Type
	// add type=jpp-stream(;ptype=ext) or type=jpt-stream;ttype=ext

	if( target != null){
	    if( !urlstring.endsWith("?"))
		urlstring = urlstring.concat( "&");
	    urlstring = urlstring.concat( "target=" + target);
	}
	if( reqtid != null){
	    if( !urlstring.endsWith("?"))
		urlstring = urlstring.concat( "&");
	    urlstring = urlstring.concat( "tid=" + reqtid);
	}
	if( reqfw != -1 && reqfh != -1){
	    if( !urlstring.endsWith("?"))
		urlstring = urlstring.concat( "&");
	    urlstring = urlstring.concat( "fsiz=" + reqfw + "," + reqfh);
	}
	if( reqrx != -1 && reqry != -1){
	    if( !urlstring.endsWith("?"))
		urlstring = urlstring.concat( "&");
	    urlstring = urlstring.concat( "roff=" + reqrx + "," + reqry);
	}
	if( reqrw != -1 && reqrh != -1){
	    if( !urlstring.endsWith("?"))
		urlstring = urlstring.concat( "&");
	    urlstring = urlstring.concat( "rsiz=" + reqrw + "," + reqrh);
	}
	if( reqcid != null){
	    if( !urlstring.endsWith("?"))
		urlstring = urlstring.concat( "&");
	    urlstring = urlstring.concat( "cid=" + reqcid);
	}
	if( reqcnew){
	    if( !urlstring.endsWith("?"))
		urlstring = urlstring.concat( "&");
	    if( reqaux == 1)
		urlstring = urlstring.concat( "cnew=http-tcp");
	    else if( reqaux == 2)
		urlstring = urlstring.concat( "cnew=http-udp");
	    else
		urlstring = urlstring.concat( "cnew=http");
	}
	if( reqJPP && !JPTstream){
	    if( !urlstring.endsWith("?"))
		urlstring = urlstring.concat( "&");
	    urlstring = urlstring.concat( "type=jpp-stream");
	}
	else if( reqJPT && !JPPstream){
	    if( !urlstring.endsWith("?"))
		urlstring = urlstring.concat( "&");
	    urlstring = urlstring.concat( "type=jpt-stream");
	}
	else{ // remove this option later
	    if( !urlstring.endsWith("?"))
		urlstring = urlstring.concat( "&");
	    if( JPTstream)
		urlstring = urlstring.concat( "type=jpt-stream");
	    else if( JPPstream)
		urlstring = urlstring.concat( "type=jpp-stream");
	}

	return urlstring;
    }
    
    public void closeChannel()
    {
	if( cid == null)
	    return;
      
	try{
	    URL url = new URL( comURL + "cclose=" + cid);
	    System.err.println( "closing cid: " + cid);
      
	    HttpURLConnection urlconn = (HttpURLConnection)url.openConnection();
	    urlconn.setRequestMethod("GET");
	    urlconn.setInstanceFollowRedirects(false);
	    urlconn.connect();
      
	    Map headers = urlconn.getHeaderFields();
     
	    urlconn.disconnect();
	} catch ( MalformedURLException e){
	    e.printStackTrace();
	} catch ( IOException e){
	    e.printStackTrace();
	}
    }    
}
