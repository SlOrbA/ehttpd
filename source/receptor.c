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
  char request_line[RCVBUFSIZE];
  size_t request_line_length;

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

  line = strchr(tcp_buffer, '\n');
  if (line == NULL) return req;
  request_line_length = (size_t)(line - tcp_buffer);
  if (request_line_length > 0 && tcp_buffer[request_line_length - 1] == '\r')
    request_line_length--;
  if (request_line_length == 0 || request_line_length >= sizeof(request_line))
    return req;
  memcpy(request_line, tcp_buffer, request_line_length);
  request_line[request_line_length] = '\0';

  {
    char extra;
    if (sscanf(request_line, "%5s %1023s HTTP/%5s %c",
               req.method, req.target, req.version, &extra) != 3)
      return req;
  }
  if (strcmp(req.version, "1.1") != 0) {
    req.status_code = 505;
    return req;
  }

  line = strstr(tcp_buffer, "\n");
  while (line != NULL && line < header_end) {
    char *next = strchr(line + 1, '\n');
    char *value = line + 1;
    char *line_end = next != NULL ? next : header_end;
    char *colon = strchr(value, ':');
    if (colon == NULL || colon >= line_end || colon == value) {
      req.status_code = 400;
      return req;
    }
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
