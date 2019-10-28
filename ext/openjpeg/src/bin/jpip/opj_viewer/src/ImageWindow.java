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
import java.awt.event.*;
import java.awt.*;

public class ImageWindow extends JFrame
{
    private ImageViewer imgviewer;
    private ImageManager imgmanager;
    
    public ImageWindow( String uri, String j2kfilename, String host, int port, boolean session, boolean jppstream, int aux)
    {
	super( j2kfilename);

	imgmanager = new ImageManager( uri, host, port);
	
	imgviewer = new ImageViewer( j2kfilename, imgmanager, session, jppstream, aux);
	imgviewer.setOpaque(true); //content panes must be opaque

	JPanel panel = new JPanel();
	panel.setLayout(new BorderLayout());
	panel.add( imgviewer, BorderLayout.CENTER);

	setContentPane( panel);
        
	addWindowListener(new WindowMyAdapter());
    }

    class WindowMyAdapter extends WindowAdapter
    {
	public void windowClosing(WindowEvent arg)
	{
	    imgmanager.closeChannel();
	    System.exit(0);
	}
    }

    public static void main(String s[])
    {
	String j2kfilename, uri, host;
	boolean session, jppstream;
	int port, aux; // 0: none, 1: tcp, 2: udp
	
	if(s.length >= 2){
	    uri = s[0];
	    j2kfilename = s[1];
	    
	    if( s.length > 2)
		host = s[2];
	    else
		host = "localhost";
	    
	    if( s.length > 3)
		port = Integer.valueOf( s[3]).intValue();
	    else
		port = 50000;
	    
	    if( s.length > 4)
		session = !s[4].equalsIgnoreCase( "stateless");
	    else
		session = true;
	    
	    if( s.length > 5)
		jppstream = !s[5].equalsIgnoreCase( "JPT");
	    else
		jppstream = true;
	    
	    if( s.length > 6){
		if( s[6].equalsIgnoreCase("udp"))
		    aux = 2;
		else
		    aux = 1;
	    }
	    else
		aux = 0;
	}
	else{
	    System.out.println("Usage: java -jar opj_viewer.jar HTTP_server_URI imagefile.jp2 [hostname] [portnumber] [stateless/session] [JPT/JPP] [tcp/udp]");
	    return;
	}
	ImageWindow frame = new ImageWindow( uri, j2kfilename, host, port, session, jppstream, aux);
    
	frame.setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);
   
	//Display the window.
	frame.pack();
	frame.setSize(new Dimension(400,200));
	frame.setLocation( 0, 50);
	frame.setVisible(true);
    }
}
