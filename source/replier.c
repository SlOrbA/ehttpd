/* ehttpd Lari Korpi lari.korpi@iki.fi (c) 2006 */
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <replier.h>

static int send_all(int socket_fd, const char *data, size_t length)
{
  size_t sent = 0;
  while (sent < length) {
    ssize_t result = send(socket_fd, data + sent, length - sent, 0);
    if (result <= 0) return -1;
    sent += (size_t)result;
  }
  return 0;
}

int http_replier(int client_socket, char *page, int status_code, int head_only)
{
  char http_header[1024];
  char date_buf[64];
  time_t now = time(NULL);
  struct tm *gmt;
  size_t header_length;
  size_t page_length;
  const char *allow_header = "";

  if (now == (time_t)-1) {
    fprintf(stderr, "No time received.\n");
    return 0;
  }
  gmt = gmtime(&now);
  if (gmt == NULL || strftime(date_buf, sizeof(date_buf),
                              "%a, %d %b %Y %H:%M:%S GMT", gmt) == 0)
    return 0;

  if (status_code < 200 || status_code >= 300 || page[0] == '\0') {
    const char *reason;
    if (status_code == 200) status_code = 404;
    reason = "Internal Server Error";
    if (status_code == 400) reason = "Bad Request";
    else if (status_code == 404) reason = "Not Found";
    else if (status_code == 405) reason = "Method Not Allowed";
    else if (status_code == 505) reason = "HTTP Version Not Supported";
    if (status_code == 405) allow_header = "Allow: GET, HEAD, POST\r\n";
    snprintf(page, 1024 * 1024,
             "<html>\n<head><title>HTTP %d %s</title></head>\n"
             "<body><h1>HTTP %d %s</h1></body>\n</html>\n",
             status_code, reason, status_code, reason);
    snprintf(http_header, sizeof(http_header),
             "HTTP/1.1 %d %s\r\n", status_code, reason);
  } else {
    snprintf(http_header, sizeof(http_header), "HTTP/1.1 200 OK\r\n");
  }
  snprintf(http_header + strlen(http_header),
           sizeof(http_header) - strlen(http_header),
           "Date: %s\r\nServer: eHTTPd/beta\r\n%sContent-Length: %zu\r\n"
           "Connection: close\r\nContent-Type: text/html; charset=utf-8\r\n\r\n",
           date_buf, allow_header, strlen(page));

  header_length = strlen(http_header);
  page_length = strlen(page);
  if (send_all(client_socket, http_header, header_length) < 0 ||
      (!head_only && send_all(client_socket, page, page_length) < 0)) {
    fprintf(stderr, "Could not send complete HTTP response.\n");
    return 0;
  }
  return (int)(header_length + page_length);
}
