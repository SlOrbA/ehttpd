/* ehttpd Lari Korpi lari.korpi@iki.fi (c) 2006 */
#ifndef _DEF_HTTP_REQUEST_
#define _DEF_HTTP_REQUEST_

typedef struct {
  unsigned char parser_pass;
  char version[6];
  char method[6];
  char host[255];
  char target[1024];
  char referer[255];
  char useragent[255];
} http_request;

#endif
