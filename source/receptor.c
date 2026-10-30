/* ehttpd Lari Korpi lari.korpi@iki.fi (c) 2006 */
#include <receptor.h>
#include <booleans.h>
#include <http.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>

static void copy_header_value(char *destination, size_t size, const char *value)
{
  while (*value == ' ' || *value == '\t') value++;
  snprintf(destination, size, "%s", value);
  destination[strcspn(destination, "\r\n")] = '\0';
}

http_request http_receptor(int client_socket)
{
  char tcp_buffer[RCVBUFSIZE];
  http_request req;
  size_t received = 0;
  ssize_t rv;
  char *header_end;
  char *line;

  memset(&req, 0, sizeof(req));
  req.parser_pass = FALSE;
  req.status_code = 400;
  memset(tcp_buffer, 0, sizeof(tcp_buffer));

  while (received < sizeof(tcp_buffer) - 1) {
    rv = recv(client_socket, tcp_buffer + received,
              sizeof(tcp_buffer) - 1 - received, 0);
    if (rv <= 0) return req;
    received += (size_t)rv;
    tcp_buffer[received] = '\0';
    if (strstr(tcp_buffer, "\r\n\r\n") || strstr(tcp_buffer, "\n\n")) break;
  }

  header_end = strstr(tcp_buffer, "\r\n\r\n");
  if (header_end == NULL) header_end = strstr(tcp_buffer, "\n\n");
  if (header_end == NULL) return req;

  if (sscanf(tcp_buffer, "%5s %1023s HTTP/%5s",
             req.method, req.target, req.version) != 3) return req;
  if (strcmp(req.version, "1.1") != 0) {
    req.status_code = 505;
    return req;
  }

  line = strstr(tcp_buffer, "\n");
  while (line != NULL && line < header_end) {
    char *next = strchr(line + 1, '\n');
    char *value = line + 1;
    if (strncasecmp(value, "Host:", 5) == 0)
      copy_header_value(req.host, sizeof(req.host), value + 5);
    else if (strncasecmp(value, "User-Agent:", 11) == 0)
      copy_header_value(req.useragent, sizeof(req.useragent), value + 11);
    else if (strncasecmp(value, "Referer:", 8) == 0)
      copy_header_value(req.referer, sizeof(req.referer), value + 8);
    line = next;
  }

  if (req.host[0] == '\0') {
    req.status_code = 400;
    return req;
  }
  if (strcasecmp(req.method, "GET") != 0) {
    req.status_code = 405;
    return req;
  }
  req.status_code = 200;
  req.parser_pass = TRUE;
  return req;
}
