/* ehttpd Lari Korpi lari.korpi@iki.fi (c) 2006 */
#ifndef _DEF_HTML_PAGE_
#define _DEF_HTML_PAGE_
#include <http.h>
int html_menu(http_request req, char *page);
int html_choice(http_request req, char *page);
#endif
