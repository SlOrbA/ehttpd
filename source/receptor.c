/* ehttpd Lari Korpi lari.korpi@iki.fi (c) 2006 */
#include <receptor.h>
#include <booleans.h>
#include <http.h>
#include <string.h>

http_request http_receptor(int client_socket)
{
  unsigned char tcp_buffer[RCVBUFSIZE]={0},end=FALSE;

  unsigned char param[255]={0},value[225]={0};

  http_request req;

  unsigned int rv=0,received=0,i;

  memset(&req,0,sizeof(req));

  req.parser_pass=TRUE;


  do {
    rv=recv(client_socket, tcp_buffer+received, RCVBUFSIZE, 0);

    if (rv < 0) {
      exit(1);
    } else if (rv==0) {
      exit(1);
    }

    received += rv;

    if(tcp_buffer[received-1]=='\n')
      {
	if(tcp_buffer[received-2]=='\n'||tcp_buffer[received-3]=='\n')
	  {
	    end=TRUE;
	  }
      }

  } while (!end);

  if(sscanf(tcp_buffer,"%s %s HTTP/%s",req.method,req.target,req.version)) {
    req.parser_pass=FALSE;
  }
  
  if(strcmp(param,"Host")==0) {
    strcpy(req.host,value);
  } else if(strcmp(param,"User-Agent")==0) {
    strcpy(req.useragent,value);
  } else if(strcmp(param,"Referer")==0) {
    strcpy(req.referer,value);
  } else {
    req.parser_pass=FALSE;
  }


  return req;
}
