/* ehttpd Lari Korpi lari.korpi@iki.fi (c) 2006 */
#include <http.h>

/* Recieving Buffer Size */
#ifndef RCVBUFSIZE 
#define RCVBUFSIZE 1024
#endif

#ifndef _DEF_HTTP_RECEPTOR_
#define _DEF_HTTP_RECEPTOR_
http_request http_receptor(int client_socket);
#endif
