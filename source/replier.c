/* ehttpd Lari Korpi lari.korpi@iki.fi (c) 2006 */
#include <stdio.h>
#include <replier.h>
#include <time.h>

/* http_replier() is used to formulate http headers and to capsulate the served page into http reply */

int http_replier(int client_socket, char *page)
{
  int reply_size=0; /* reply_size will hold the size of the http reply in bytes */
  char http_header[1024]={0}; /* http_header[] is buffer for header constructing */
  
  time_t now; /* a timestamp for the headers */

  now=time(NULL); /* current time is written to now */

  if(now == (time_t *)0) /* null pointer check sures that we have a timestamp */
    {
      sprintf(stderr,"No time recieved.\n");
      return 0;
    }

  sprintf(stderr,"Replied %s\n",ctime(&now));
 
  
  if (strlen(page)) 
    {
      sprintf(http_header,"HTTP/1.1 200 OK\n");
    }
  else
    {
      sprintf(http_header,"HTTP/1.1 404 Not Found\n");
      sprintf(page,"<html>\n<head>\n<title>HTTP 404 Not Found</title>\n</head>\n<body><h1>Requested URL location is not available.</h1>\n</body>\n</html>\n");
    }
  sprintf(http_header+strlen(http_header),"Date: %s",ctime(&now));
  sprintf(http_header+strlen(http_header),"Server: eHTTPd/beta\n");
  sprintf(http_header+strlen(http_header),"Content-Length: %d\n",strlen(page));
  sprintf(http_header+strlen(http_header),"Connection: close\n");
  sprintf(http_header+strlen(http_header),"Content-Type: text/html\n\n");

  if(send(client_socket, http_header, strlen(http_header), 0) != strlen(http_header))
    {
      sprintf(stderr,"Problem in http header send!\n");
    }
  else 
    {
      reply_size = strlen(http_header);
    }

   if(send(client_socket, page, strlen(page), 0) != strlen(page) )
    { 
      sprintf(stderr,"(S)He didn't hear me!\n");
    }
  else 
    {
      reply_size = reply_size + strlen(page);
    }
   
   return reply_size;

}
