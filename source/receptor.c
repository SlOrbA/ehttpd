/* ehttpd Lari Korpi lari.korpi@iki.fi (c) 2006 */
#include <receptor.h>
#include <booleans.h>
#include <http.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <sys/socket.h>

static void copy_header_value(char *destination, size_t size, const char *value)
{
  while (*value == ' ' || *value == '\t') value++;
  snprintf(destination, size, "%s", value);
  destination[strcspn(destination, "\r\n")] = '\0';
}

static int is_token_char(unsigned char character)
{
  if (isalnum(character)) return 1;
  return strchr("!#$%&'*+-.^_`|~", character) != NULL;
}

static int valid_field_name(const char *start, const char *end)
{
  const char *character;
  if (start == end) return 0;
  for (character = start; character < end; character++) {
    if (!is_token_char((unsigned char)*character)) return 0;
  }
  return 1;
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
  int host_seen = 0;
  size_t content_length = 0;
  size_t header_size;

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
  header_size = (size_t)(header_end - tcp_buffer) +
                (header_end[0] == '\r' ? 4 : 2);

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
    char *line_end = next != NULL && next < header_end ? next : header_end;
    char *colon = strchr(value, ':');
    if (colon == NULL || colon >= line_end ||
        !valid_field_name(value, colon)) {
      req.status_code = 400;
      return req;
    }
    if (strncasecmp(value, "Host:", 5) == 0) {
      if (host_seen) {
        req.status_code = 400;
        return req;
      }
      host_seen = 1;
      copy_header_value(req.host, sizeof(req.host), value + 5);
    } else if (strncasecmp(value, "User-Agent:", 11) == 0)
      copy_header_value(req.useragent, sizeof(req.useragent), value + 11);
    else if (strncasecmp(value, "Referer:", 8) == 0)
      copy_header_value(req.referer, sizeof(req.referer), value + 8);
    else if (strncasecmp(value, "Content-Length:", 15) == 0) {
      char *end;
      unsigned long length = strtoul(value + 15, &end, 10);
      if (end == value + 15 || end > line_end || length >= sizeof(req.body))
        return req;
      while (end < line_end && (*end == ' ' || *end == '\t' || *end == '\r')) end++;
      if (end != line_end) return req;
      content_length = (size_t)length;
    }
    line = next;
  }

  if (req.host[0] == '\0') {
    req.status_code = 400;
    return req;
  }
  if (strcmp(req.method, "GET") != 0 && strcmp(req.method, "HEAD") != 0 &&
      strcmp(req.method, "POST") != 0) {
    req.status_code = 405;
    return req;
  }
  if (strcmp(req.method, "POST") == 0) {
    while (received < header_size + content_length &&
           received < sizeof(tcp_buffer) - 1) {
      rv = recv(client_socket, tcp_buffer + received,
                sizeof(tcp_buffer) - 1 - received, 0);
      if (rv <= 0) return req;
      received += (size_t)rv;
    }
    if (content_length == 0 || received < header_size + content_length) return req;
    memcpy(req.body, tcp_buffer + header_size, content_length);
    req.body[content_length] = '\0';
  }
  req.status_code = 200;
  req.parser_pass = TRUE;
  return req;
}
