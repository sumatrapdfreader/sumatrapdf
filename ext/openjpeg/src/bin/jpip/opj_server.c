/*
 * $Id: opj_server.c 53 2011-05-09 16:55:39Z kaori $
 *
 * Copyright (c) 2002-2014, Universite catholique de Louvain (UCL), Belgium
 * Copyright (c) 2002-2014, Professor Benoit Macq
 * Copyright (c) 2010-2011, Kaori Hagihara
 * Copyright (c) 2011,      Lucian Corlaciu, GSoC
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

/*! \file
 *  \brief opj_server is a JPIP server program, which supports HTTP connection, JPT-stream, session, channels, and cache model managements.
 *
 *  \section req Requirements
 *    FastCGI development kit (http://www.fastcgi.com).
 *
 *  \section impinst Implementing instructions
 *  Launch opj_server from the server terminal:\n
 *   % spawn-fcgi -f ./opj_server -p 3000 -n
 *
 *  Note: JP2 files are stored in the working directory of opj_server\n
 *  Check README for the JP2 Encoding\n
 *
 *  We tested this software with a virtual server running on the same Linux machine as the clients.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "fcgi_stdio.h"
#include "openjpip.h"

#ifndef QUIT_SIGNAL
#define QUIT_SIGNAL "quitJPIP"
#endif

#ifdef _WIN32
WSADATA initialisation_win32;
#endif /*_WIN32*/

int main(void)
{
    server_record_t *server_record;
#ifdef SERVER
    char *query_string;
#endif

#ifdef _WIN32
    int erreur = WSAStartup(MAKEWORD(2, 2), &initialisation_win32);
    if (erreur != 0) {
        fprintf(stderr, "Erreur initialisation Winsock error : %d %d\n", erreur,
                WSAGetLastError());
    } else {
        fprintf(stderr, "Initialisation Winsock\n");
    }
#endif /*_WIN32*/

    server_record = init_JPIPserver(60000, 0);

#ifdef SERVER
    while (FCGI_Accept() >= 0)
#else

    char query_string[128];
    while (fgets(query_string, 128, stdin) && query_string[0] != '\n')
#endif
    {
        QR_t *qr;
        OPJ_BOOL parse_status;

#ifdef SERVER
        query_string = getenv("QUERY_STRING");
#endif /*SERVER*/

        if (strcmp(query_string, QUIT_SIGNAL) == 0) {
            break;
        }

        qr = parse_querystring(query_string);

        parse_status = process_JPIPrequest(server_record, qr);

#ifndef SERVER
        local_log(OPJ_TRUE, OPJ_TRUE, parse_status, OPJ_FALSE, qr, server_record);
#endif

        if (parse_status) {
            send_responsedata(server_record, qr);
        } else {
            fprintf(FCGI_stderr, "Error: JPIP request failed\n");
            fprintf(FCGI_stdout, "\r\n");
        }

        end_QRprocess(server_record, &qr);
    }

    fprintf(FCGI_stderr, "JPIP server terminated by a client request\n");

    terminate_JPIPserver(&server_record);

#ifdef _WIN32
    if (WSACleanup() != 0) {
        fprintf(stderr, "\nError in WSACleanup : %d %d", erreur, WSAGetLastError());
    } else {
        fprintf(stderr, "\nWSACleanup OK\n");
    }
#endif

    return 0;
}
