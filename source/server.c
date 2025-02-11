/* ehttpd Lari Korpi lari.korpi@iki.fi (c) 2006 */
#include <stdio.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <http.h>
#include <receptor.h>
#include <replier.h>
#include <reply.h>

#define MAXPENDING 7

/*********************/
/*                   */
/* The Main Function */
/*                   */
/*********************/

int main(int argc, char *argv[]) {

  int server_socket; /* Listener socket */
  int client_socket; /* Request handler socket */

  char page[1024*1024]={0}; /* Pointer to HTML page*/

  int length=0; /* Initialising reponse lenght */

  http_request req;

  struct sockaddr_in http_server_address;
  struct sockaddr_in http_client_address;

  unsigned short ip_version; /* IP Protocl version */
  
  unsigned short http_server_port; /* Port Number for server */

  unsigned int client_length; /* Length of client's ip address */

  pid_t PID;
  unsigned int child_process_count = 0;

  if (argc != 3) {
    fprintf(stderr, "Usage: %s <4/6> <Server Port>\n", argv[0]);
    exit(1);
  }

  ip_version = atoi(argv[1]);
  http_server_port = atoi(argv[2]);

  if ((server_socket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
      printf("Server socket not obatined 00\n");
      exit(1);
    }

  memset(&http_server_address, 0, sizeof(http_server_address));
  if (ip_version == 4)
    http_server_address.sin_family = AF_INET;
  else if (ip_version == 6)
    http_server_address.sin_family = AF_INET6;
  else {
    fprintf(stderr, "Default to IP version 6\n");
    http_server_address.sin_family = AF_INET6;
  }

  http_server_address.sin_addr.s_addr = htonl(INADDR_ANY);

  if (http_server_port == 0) {
    fprintf(stderr, "Default to port 8080\n");
    http_server_port = 8080;
  }

  http_server_address.sin_port = htons(http_server_port);

  if (bind(server_socket, (struct sockaddr *) &http_server_address, sizeof(http_server_address)) < 0) {
      fprintf(stderr, "Server socket not binded 01\n");
      exit(1);
    }

  if (listen(server_socket, MAXPENDING) < 0) {
      fprintf(stderr, "Server couldn't listen 02\n");
      exit(1);
    }

  fprintf(stderr, "Ready serve requests\n");

  for(;;) { /* Forever loop */
    client_length = sizeof(http_client_address);
    
    if ((client_socket = accept(server_socket, (struct sockaddr *) &http_client_address, &client_length)) < 0) {
      fprintf(stderr, "Server couldn't accept connection 03\n");
      exit(1);
    }
    if ((PID = fork()) < 0) {
      fprintf(stderr, "Couldn't do fork\n");
      exit(1);
    }
    else if (PID == 0) {

      int reply_size=0;

      fprintf(stderr, "Request registered\n");  
      close(server_socket);
      fprintf(stderr, "Server socket closed\n");  
      req=http_receptor(client_socket);
      fprintf(stderr, "%s requested\n",req.target);
      
      if (strncmp(req.target,"/choice",(size_t)7) == 0) {
	fprintf(stderr, "/choice is being read\n");
	html_choice(req,page);
      }
      else if (strncmp(req.target,"/favicon.ico",(size_t)7) == 0){
	fprintf(stderr, "/favicon.ico is being read\n");
      } else {
	fprintf(stderr, "default is being read\n");
	html_menu(req,page);
      }
      reply_size = http_replier(client_socket,page);
      fprintf(stderr, "%d chars sent.\n",reply_size);
      close(client_socket);
      fprintf(stderr, "Client socket closed\n");
      fprintf(stderr, "Server prosess exiting\n");
      
      fflush(stderr);
      exit(0);
    }
    close(client_socket);
    child_process_count++;
    
    while(child_process_count) {
      PID = waitpid((pid_t) -1, NULL, WNOHANG);
      if (PID < 0) {
	fprintf(stderr, "Major error 01: Dased and confused\n");
	exit(1);
      } else if (PID == 0) break;
      else child_process_count--;
    }
  }
}
