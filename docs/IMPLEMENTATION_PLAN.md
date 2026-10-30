# Tarkka toteutussuunnitelma (Implementation Plan for AI / Gemini Flash)

Tämä dokumentti sisältää **yksityiskohtaiset, tehtäväkohtaiset toteutusohjeet** [`ROADMAP.md`](./ROADMAP.md)-suunnitelman tehtäville.

> **Ohje toteuttavalle tekoälymallille (esim. Gemini Flash):**
> * Toteuta **yksi tehtävä kerrallaan** alla olevassa järjestyksessä.
> * Noudata tarkasti annettuja tiedostopolkuja, funktiomäärittelyjä ja C-kielisiä rakenteita.
> * Jokaisen tehtävän jälkeen aja kohdassa **"Todennus (Verification)"** mainitut komennot varmistaaksesi, että koodi kääntyy ilman virheitä.

---

## Vaihe 1: Kriittiset bugikorjaukset ja muistiturvallisuus

### Tehtävä 1.1: Korjaa `source/replier.c` (`fprintf`, `time_t` ja puskurirajat)

* **Kohdetiedostot**: `source/replier.c`, `source/replier.h`
* **Ongelma**:
  1. Riveillä 19, 23, 43 ja 52 kutsutaan `sprintf(stderr, ...)`, vaikka ensimmäinen argumentti on `FILE *stderr`. Tämän kuuluu olla `fprintf(stderr, ...)`.
  2. Rivillä 17 verrataan `now == (time_t *)0`, vaikka `time_t` on kokonaislukutyyppi ja `time(NULL)` palauttaa `-1` (`(time_t)-1`) virhetilanteessa.
  3. Puuttuvat otsikkotiedostot `<string.h>` ja `<sys/socket.h>`.
  4. `sprintf` voi aiheuttaa ylivuodon `http_header`-puskuriin; käytä `snprintf`-funktiota.
* **Toteutusohje**:
  Korvaa `source/replier.c` käyttämään seuraavaa logiikkaa:
  ```c
  /* ehttpd Lari Korpi lari.korpi@iki.fi (c) 2006 */
  #include <stdio.h>
  #include <string.h>
  #include <sys/socket.h>
  #include <time.h>
  #include <replier.h>

  int http_replier(int client_socket, char *page)
  {
    int reply_size = 0;
    char http_header[1024] = {0};
    char date_buf[64] = {0};
    time_t now;
    struct tm gm_now;
    const char *status_line;

    now = time(NULL);
    if (now == (time_t)-1) {
      fprintf(stderr, "No time received.\n");
      return 0;
    }

    gmtime_r(&now, &gm_now);
    strftime(date_buf, sizeof(date_buf), "%a, %d %b %Y %H:%M:%S GMT", &gm_now);
    fprintf(stderr, "Replied %s\n", date_buf);

    if (strlen(page) > 0) {
      status_line = "HTTP/1.1 200 OK\r\n";
    } else {
      status_line = "HTTP/1.1 404 Not Found\r\n";
      strcpy(page,
        "<html>\n<head>\n<title>HTTP 404 Not Found</title>\n</head>\n"
        "<body><h1>Requested URL location is not available.</h1>\n</body>\n</html>\n");
    }

    snprintf(http_header, sizeof(http_header),
      "%s"
      "Date: %s\r\n"
      "Server: eHTTPd/0.04\r\n"
      "Content-Length: %zu\r\n"
      "Connection: close\r\n"
      "Content-Type: text/html; charset=utf-8\r\n\r\n",
      status_line, date_buf, strlen(page));

    size_t header_len = strlen(http_header);
    if ((size_t)send(client_socket, http_header, header_len, 0) != header_len) {
      fprintf(stderr, "Problem in http header send!\n");
    } else {
      reply_size = (int)header_len;
    }

    size_t page_len = strlen(page);
    if ((size_t)send(client_socket, page, page_len, 0) != page_len) {
      fprintf(stderr, "(S)He didn't hear me!\n");
    } else {
      reply_size += (int)page_len;
    }

    return reply_size;
  }
  ```
* **Todennus**: Käännä `gcc -Wall -Wextra -I./source -c source/replier.c -o /tmp/replier.o`. Ei saa tulla yhtäkään varoitusta.

---

### Tehtävä 1.2 & 1.3: Korjaa muistivirheet, Path Traversal ja tiedoston tallennus tiedostossa `source/reply.c`

* **Kohdetiedostot**: `source/reply.c`, `source/reply.h`
* **Ongelmat**:
  1. `char cwd[] = ".";` on vain 2 tavun kokoinen, joten `strcat(cwd, req.target)` kirjoittaa pinon yli! Lisäksi riveillä 22 ja 27 `strcat` kutsutaan kahdesti peräkkäin, jolloin polku kahdentuu (`./target/target`).
  2. `stat(working_directory_ent->d_name, &file_stat)` rivillä 44 tarkistaa vain nykyisen työhakemiston, ei alihakemiston polkua.
  3. `opendir()`-kahvaa ei koskaan suljeta `closedir(working_directory)`-kutsulla.
  4. Tiedoston muokkauksessa (`html_choice`, rivit 163–205) `feof(fd)`-silmukka lukee ja kirjoittaa `EOF` (`-1`) -tavuja tiedostoon, eikä katkaise tiedostoa (`ftruncate`), jos uusi rivi on lyhyempi kuin vanha.
  5. Path Traversal -haavoittuvuus: `req.target` tai `file_path` voi sisältää `..`, jolloin käyttäjä pääsee käsiksi esim. `/etc/passwd`-tiedostoon.
* **Toteutusohje**:
  1. Päivitä `source/reply.h` sisältämään oikeat prototyypit:
     ```c
     #ifndef _DEF_HTML_PAGE_
     #define _DEF_HTML_PAGE_
     #include <http.h>
     int html_menu(http_request req, char *page);
     int html_choice(http_request req, char *page);
     #endif
     ```
  2. Lisää `source/reply.c`-tiedostoon apufunktio turvallisen polun muodostamiseen:
     ```c
     static int build_safe_path(const char *rel_path, char *out_path, size_t out_size) {
       if (rel_path == NULL || strstr(rel_path, "..") != NULL) {
         return -1; /* Estä Path Traversal */
       }
       if (rel_path[0] == '/') {
         snprintf(out_path, out_size, ".%s", rel_path);
       } else {
         snprintf(out_path, out_size, "./%s", rel_path);
       }
       return 0;
     }
     ```
  3. Korvaa `html_menu`:ssa `strcat(cwd, req.target)` kutsumalla `build_safe_path(req.target, full_path, sizeof(full_path))`. Muodosta jokaiselle hakemistomerkinnälle polku `snprintf(entry_path, sizeof(entry_path), "%s/%s", full_path, working_directory_ent->d_name)` ennen `stat()`-kutsua. Kutsu lopuksi `closedir(working_directory)`.
  4. Korjaa `html_choice`:n tiedoston tallennus käyttämään `int ch; while ((ch = fgetc(fd)) != EOF)` -rakennetta `feof()`-silmukoiden sijaan. Avaa tiedosto uudelleenkirjoitusta varten `"w"`-tilassa, kun väliaikaistiedostosta (`fd_tmp`) kopioidaan sisältö takaisin alkuperäiseen tiedostoon, jotta vanhaa häntää ei jää tiedoston loppuun.
* **Todennus**: Käännä `gcc -Wall -Wextra -I./source -c source/reply.c -o /tmp/reply.o`.

---

### Tehtävä 1.4: Suojaa `source/receptor.c` ja jäsennä HTTP-pyyntö oikein

* **Kohdetiedostot**: `source/receptor.c`, `source/receptor.h`
* **Ongelmat**:
  1. `recv(client_socket, tcp_buffer + received, RCVBUFSIZE, 0)` voi ylivuotaa `tcp_buffer`-taulukon, jos data saapuu useassa osassa.
  2. `received - 2` ja `received - 3` aiheuttavat alivuodon (underflow), jos `received < 3`.
  3. `sscanf(tcp_buffer, "%s %s HTTP/%s", ...)` ei rajoita merkkijonojen pituutta.
  4. Otsikoita (`Host`, `User-Agent`, `Referer`) ei koskaan lueta `tcp_buffer`-puskurista `param`- ja `value`-muuttujiin.
* **Toteutusohje**:
  1. Lue `recv`-silmukassa enintään `RCVBUFSIZE - 1 - received` tavua ja varmista aina `tcp_buffer[received] = '\0'`.
  2. Tarkista otsikoiden päättyminen etsimällä `strstr((char *)tcp_buffer, "\r\n\r\n")` tai `strstr((char *)tcp_buffer, "\n\n")`.
  3. Käytä pituusrajoitettua `sscanf`-kutsua:
     ```c
     if (sscanf((char *)tcp_buffer, "%5s %1023s HTTP/%5s", req.method, req.target, req.version) != 3) {
       req.parser_pass = FALSE;
       return req;
     }
     ```
  4. Käy otsikkorivit läpi rivi kerrallaan (`strtok_r` tai riviosoittimilla) ja poimi `Host:`, `User-Agent:` ja `Referer:` turvallisesti `strncpy`-funktiolla `req`-rakenteeseen.
* **Todennus**: Käännä `gcc -Wall -Wextra -I./source -c source/receptor.c -o /tmp/receptor.o`.

---

## Vaihe 2: Palvelimen verkkokäsittely ja uuden tiedoston luonti

### Tehtävä 2.1: Korjaa IPv4/IPv6 ja `SO_REUSEADDR` tiedostossa `source/server.c`

* **Kohdetiedosto**: `source/server.c`
* **Ongelmat**:
  1. `socket(PF_INET, ...)` ja `struct sockaddr_in` ovat kiinteästi IPv4-rakenteita, vaikka käyttäjä antaisi argumentiksi `6` (IPv6).
  2. `SO_REUSEADDR`-valitsin puuttuu, minkä vuoksi palvelimen uudelleenkäynnistys epäonnistuu (`Server socket not binded 01`) `TIME_WAIT`-tilan aikana.
  3. `/favicon.ico`-vertailussa `strncmp(req.target, "/favicon.ico", 7)` tarkistaa vain 7 ensimmäistä merkkiä (`12`:n sijaan).
* **Toteutusohje**:
  1. Lisää `setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))` ennen `bind()`-kutsua.
  2. Käytä `struct sockaddr_storage`-rakennetta tai erillisiä `struct sockaddr_in` (IPv4) ja `struct sockaddr_in6` (IPv6) -rakenteita sekä `PF_INET6`-perhettä, kun `ip_version == 6`.
  3. Korjaa `/favicon.ico`-tarkistus pituuteen `12`.

### Tehtävä 2.2: Toteuta uuden tiedoston luonti (`/choice?new=...`) tiedostossa `source/reply.c`

* **Kohdetiedosto**: `source/reply.c`
* **Ongelma**: `html_menu` luo linkin `<a href='/choice?new=%s'>Add new file</a>`, mutta `html_choice` olettaa aina parametrin olevan `edit=`.
* **Toteutusohje**:
  1. Tarkista `html_choice`-funktiossa, alkaako kyselymerkkijono `?new=` vai `?edit=`.
  2. Jos kyseessä on `?new=<dir>` ilman `&file=`-parametria, palauta HTML-lomake, jossa käyttäjä voi syöttää uuden tiedoston nimen.
  3. Jos kyseessä on `?new=<dir>&file=<filename>`, tarkista polku `build_safe_path`-funktiolla, luo tyhjä tiedosto (`fopen(path, "a")`) ja ohjaa käyttäjä muokkaamaan tiedostoa (`/choice?edit=...`).

---

## Vaihe 3: Docker ja GitHub Actions CI/CD

### Tehtävä 3.1: Päivitä `Dockerfile`

* **Kohdetiedosto**: `Dockerfile`
* **Ongelma**: Nykyinen `Dockerfile` ajaa `git clone https://github.com/SlOrbA/ehttpd.git` sen sijaan, että se käyttäisi paikallista lähdekoodia, ja `ENTRYPOINT ["ehttpd"]` epäonnistuu, koska binääri kopioidaan työhakemistoon eikä sille anneta oletusparametreja.
* **Toteutusohje**:
  ```dockerfile
  FROM alpine:3.20 AS build
  RUN apk add --no-cache build-base automake autoconf libtool openssl-dev
  WORKDIR /ehttpd
  COPY . .
  RUN autoreconf -fi && ./configure && make && make install

  FROM alpine:3.20 AS final
  RUN apk add --no-cache libssl3 libcrypto3
  COPY --from=build /usr/local/bin/ehttpd /usr/local/bin/ehttpd
  WORKDIR /srv/ehttpd
  EXPOSE 8080
  ENTRYPOINT ["/usr/local/bin/ehttpd"]
  CMD ["4", "8080"]
  ```

### Tehtävä 3.2: Lisää GitHub Actions CI (`.github/workflows/ci.yml`)

* **Kohdetiedosto**: `.github/workflows/ci.yml`
* **Toteutusohje**:
  Luo työnkulku, joka:
  1. Asentaa `autoconf`, `automake`, `libssl-dev`, `curl`.
  2. Ajaa `autoreconf -fi && ./configure CFLAGS="-Wall -Wextra" && make`.
  3. Käynnistää `./source/ehttpd 4 8080 &`, testaa `curl -fsS http://localhost:8080/` ja sammuttaa palvelimen.

---

## Vaihe 4: OpenSSL (HTTPS) -integraatio

### Tehtävä 4.1: Luo `source/net_io.h` ja `source/net_io.c`

* **Kohdetiedostot**: `source/net_io.h`, `source/net_io.c`, `source/Makefile.am`
* **Toteutusohje**:
  1. Toteuta dokumentissa [`openssl-https.md`](./openssl-https.md) kuvattu `ehttpd_conn`-tietorakenne sekä funktiot:
     * `ssize_t conn_read(ehttpd_conn *conn, void *buf, size_t len)`
     * `ssize_t conn_write(ehttpd_conn *conn, const void *buf, size_t len)`
     * `void conn_close(ehttpd_conn *conn)`
  2. Päivitä `http_receptor(ehttpd_conn *conn)` ja `http_replier(ehttpd_conn *conn, char *page)` käyttämään `conn_read`- ja `conn_write`-kutsuja `recv`/`send`-kutsujen sijaan.
  3. Lisää `net_io.c` ja `net_io.h` tiedostoon `source/Makefile.am` (`ehttpd_SOURCES`-listaan).

### Tehtävä 4.2: Päivitä `configure.ac` ja `source/server.c` tukemaan OpenSSL:ää

* **Kohdetiedostot**: `configure.ac`, `source/server.c`
* **Toteutusohje**:
  1. Lisää `configure.ac`-tiedostoon:
     ```autoconf
     AC_CHECK_HEADERS([openssl/ssl.h openssl/err.h])
     AC_CHECK_LIB([crypto], [ERR_print_errors_fp])
     AC_CHECK_LIB([ssl], [SSL_CTX_new])
     ```
  2. Päivitä `source/server.c` hyväksymään valinnaiset komentoriviparametrit varmenteelle ja avaimelle:
     `Usage: ehttpd <4/6> <Server Port> [cert.pem key.pem]`
  3. Jos `cert.pem` ja `key.pem` on annettu ja `HAVE_LIBSSL` on määritelty, alusta `SSL_CTX` ja suorita lapsiprosessissa `SSL_accept()`.

---

## Vaihe 5: HTTP/2 (`h2`) -tuki

### Tehtävä 5.1: Lisää ALPN ja `libnghttp2`-käsittelijä (`source/http2.c`, `source/http2.h`)

* **Kohdetiedostot**: `configure.ac`, `source/http2.h`, `source/http2.c`, `source/server.c`, `source/Makefile.am`
* **Toteutusohje**:
  1. Lisää `configure.ac`-tiedostoon tarkistus `libnghttp2`-kirjastolle (`AC_CHECK_LIB([nghttp2], [nghttp2_session_server_new])`).
  2. Lisää OpenSSL:n `SSL_CTX`-alustukseen ALPN-takaisinkutsu (`SSL_CTX_set_alpn_select_cb`), joka tarjoaa protokollia `h2` ja `http/1.1`.
  3. Kun `SSL_accept()` on valmis, tarkista valittu protokolla `SSL_get0_alpn_selected()`-kutsulla:
     * Jos protokolla on `"h2"`, kutsu `http2_serve_connection(&conn)` (määritelty tiedostossa `source/http2.c`), joka lukee `:method`- ja `:path`-pseudo-otsikot `http_request`-rakenteeseen, kutsuu olemassa olevia `html_menu`- tai `html_choice`-funktioita ja lähettää vastauksen HTTP/2-kehyksinä.
     * Muussa tapauksessa käytä olemassa olevaa HTTP/1.1-polkua (`http_receptor` -> `html_menu`/`html_choice` -> `http_replier`).
