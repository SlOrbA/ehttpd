# OpenSSL ja HTTPS/TLS-tuki ehttpd-palvelimessa

Tämä dokumentti kuvaa, mitä **OpenSSL** on, miten **TLS/HTTPS** toimii C-kielisessä palvelimessa ja miten se suunnitellaan osaksi **ehttpd**-arkkitehtuuria.

---

## 1. Mikä on OpenSSL?

**OpenSSL** on avoimen lähdekoodin C-kielinen ohjelmistokirjasto, joka toteuttaa **TLS** (Transport Layer Security, aiemmin SSL) -protokollat sekä laajan valikoiman kryptografisia algoritmeja. Se jakautuu kahteen pääkirjastoon:

1. **`libcrypto`**:
   * Toteuttaa matalan tason kryptografiset perusoperaatiot: symmetrisen salauksen (AES, ChaCha20), julkisen avaimen salauksen (RSA, ECDSA, Ed25519), tiivistefunktiot (SHA-256, SHA-3) ja X.509-varmenteiden käsittelyn.
2. **`libssl`**:
   * Toteuttaa TLS-protokollan (TLS 1.2 ja TLS 1.3) kättelyn, istuntojen hallinnan ja salatun tietoliikenteen `libcrypto`-kirjaston päällä.

Sulautetuissa Linux-järjestelmissä (esim. Alpine Linux, OpenWrt, Buildroot, Yocto) OpenSSL on yleensä valmiina jaettuna kirjastona, joten sen käyttäminen ei kasvata itse `ehttpd`-binäärin kokoa kuin muutamalla kilotavulla.

---

## 2. Miten HTTPS (HTTP over TLS) toimii palvelimessa?

Kun selain ottaa yhteyttä HTTPS-palvelimeen, tapahtuu seuraava ketju:

1. **TCP-kättely (3-way handshake)**:
   * Tavallinen `accept()`-kutsu luo asiakkaalle TCP-socketin (`client_socket`).
2. **TLS-kättely (`SSL_accept`)**:
   * Palvelin esittää X.509-palvelinvarmenteensa (`server.crt`) ja todistaa omistavansa sitä vastaavan yksityisen avaimen (`server.key`).
   * Asiakas ja palvelin sopivat salausalgoritmista (Cipher Suite), vaihtavat avaimet (esim. ECDHE) ja sopivat sovellusprotokollasta **ALPN**-laajennuksen avulla (`http/1.1` tai `h2`).
3. **Salattu tiedonsiirto (`SSL_read` / `SSL_write`)**:
   * Tavallisten `recv()`- ja `send()`-kutsujen sijaan palvelin lukee ja kirjoittaa dataa `SSL_read()`- ja `SSL_write()`-funktioilla.
4. **Yhteyden sulkeminen (`SSL_shutdown` & `SSL_free`)**:
   * Palvelin lähettää TLS `close_notify` -viestin, vapauttaa `SSL`-rakenteen ja sulkee lopuksi TCP-socketin (`close(client_socket)`).

---

## 3. Arkkitehtuurisuunnitelma: I/O-abstraktiokerros (`source/net_io.h` & `source/net_io.c`)

Jotta `receptor.c`:n ja `replier.c`:n ei tarvitse tietää, onko yhteys tavallinen HTTP vai salattu HTTPS, `ehttpd`-palvelimeen lisätään kevyt yhteysrakenne `ehttpd_conn`:

```c
/* source/net_io.h */
#ifndef _DEF_NET_IO_
#define _DEF_NET_IO_

#include <stddef.h>
#include <sys/types.h>

#ifdef HAVE_OPENSSL
#include <openssl/ssl.h>
#include <openssl/err.h>
#endif

typedef struct {
    int fd;               /* TCP client_socket */
    int use_tls;          /* 0 = HTTP, 1 = HTTPS */
    const char *alpn_proto; /* "http/1.1" tai "h2" */
#ifdef HAVE_OPENSSL
    SSL *ssl;             /* OpenSSL-yhteysolio */
#endif
} ehttpd_conn;

ssize_t conn_read(ehttpd_conn *conn, void *buf, size_t len);
ssize_t conn_write(ehttpd_conn *conn, const void *buf, size_t len);
void conn_close(ehttpd_conn *conn);

#endif
```

Tämän abstraktion ansiosta:
* Jos `conn->use_tls == 0`, `conn_read()` kutsuu `recv()` ja `conn_write()` kutsuu `send()`.
* Jos `conn->use_tls == 1`, `conn_read()` kutsuu `SSL_read()` ja `conn_write()` kutsuu `SSL_write()`.

---

## 4. OpenSSL:n alustus ja käyttö `ehttpd`:ssä

### 4.1 Palvelimen käynnistysvaihe (`server.c`, pääprosessi)

Ennen `for(;;)`-kuuntelusilmukkaa luodaan ja konfiguroidaan `SSL_CTX`:

```c
SSL_CTX *create_tls_context(const char *cert_file, const char *key_file) {
    const SSL_METHOD *method = TLS_server_method();
    SSL_CTX *ctx = SSL_CTX_new(method);
    if (!ctx) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    /* Vaaditaan vähintään TLS 1.2 tietoturvasyistä */
    SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION);

    /* Ladataan palvelinvarmenne ja yksityinen avain */
    if (SSL_CTX_use_certificate_file(ctx, cert_file, SSL_FILETYPE_PEM) <= 0 ||
        SSL_CTX_use_PrivateKey_file(ctx, key_file, SSL_FILETYPE_PEM) <= 0) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    return ctx;
}
```

### 4.2 Lapsiprosessin vaihe (`fork()`-kutsun jälkeen)

Koska `ehttpd` käyttää `fork()`-mallia, `SSL_CTX` alustetaan kerran pääprosessissa ja periytyy lapsiprosesseille. Lapsiprosessi luo yhteyskohtaisen `SSL *`-olion `accept()`-kutsun jälkeen:

```c
ehttpd_conn conn = { .fd = client_socket, .use_tls = tls_enabled };
if (tls_enabled) {
    conn.ssl = SSL_new(ssl_ctx);
    SSL_set_fd(conn.ssl, client_socket);
    if (SSL_accept(conn.ssl) <= 0) {
        ERR_print_errors_fp(stderr);
        conn_close(&conn);
        exit(EXIT_FAILURE);
    }
}
```

---

## 5. ALPN (Application-Layer Protocol Negotiation) HTTP/2:ta varten

Kun sekä OpenSSL että HTTP/2 ovat käytössä, palvelin rekisteröi ALPN-valintafunktion `SSL_CTX_set_alpn_select_cb`-kutsulla. Jos asiakas tukee `h2`-protokollaa, palvelin valitsee sen; muuten valitaan `http/1.1`.

---

## 6. Kehitysvarmenteen luominen testausta varten

Paikallista kehitystä ja automaattitestejä varten itseallekirjoitettu (self-signed) varmenne ja avain voidaan luoda yhdellä komennolla:

```bash
openssl req -x509 -newkey rsa:2048 -nodes \
  -keyout server.key -out server.crt \
  -days 365 -subj "/CN=localhost"
```

Testaus `curl`-komennolla:
```bash
curl -k https://localhost:8443/
```
*(Valitsin `-k` / `--insecure` ohittaa itseallekirjoitetun varmenteen tarkistuksen testauksessa.)*
