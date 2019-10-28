/*
 * $Id: opj_dec_server.c 54 2011-05-10 13:22:47Z kaori $
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

/*! \file
 *  \brief opj_dec_server is a server to decode JPT-stream and communicate locally with JPIP client, which is coded in java.
 *
 *  \section impinst Implementing instructions
 *  Launch opj_dec_server from a terminal in the same machine as JPIP client image viewers. \n
 *   % ./opj_dec_server [portnumber]\n
 *  ( portnumber=50000 by default)\n
 *  Keep it alive as long as image viewers are open.\n
 *
 *  To quite the opj_dec_server, send a message "quit" through the telnet.\n
 *   % telnet localhost 50000\n
 *     quit\n
 *  Be sure all image viewers are closed.\n
 *  Cache file in JPT format is stored in the working directly before it quites.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include "opj_config.h"
#include "openjpip.h"

#ifdef _WIN32
WSADATA initialisation_win32;
#endif

int main(int argc, char *argv[])
{

    dec_server_record_t *server_record;
    client_t client;
    int port = 50000;
    int erreur;
    (void)erreur;

    if (argc > 1) {
        port = atoi(argv[1]);
    }

#ifdef _WIN32
    erreur = WSAStartup(MAKEWORD(2, 2), &initialisation_win32);
    if (erreur != 0) {
        fprintf(stderr, "Erreur initialisation Winsock error : %d %d\n", erreur,
                WSAGetLastError());
    } else {
        printf("Initialisation Winsock\n");
    }
#endif /*_WIN32*/

    server_record = init_dec_server(port);

    while ((client = accept_connection(server_record)) != -1)
        if (!handle_clientreq(client, server_record)) {
            break;
        }

    terminate_dec_server(&server_record);

#ifdef _WIN32
    if (WSACleanup() != 0) {
        printf("\nError in WSACleanup : %d %d", erreur, WSAGetLastError());
    } else {
        printf("\nWSACleanup OK\n");
    }
#endif

    return 0;
}
