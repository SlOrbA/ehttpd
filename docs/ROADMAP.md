# ehttpd Roadmap (Kehityssuunnitelma)

Tämä tiekartta (Roadmap) määrittelee **ehttpd**-projektin kehitysvaiheet nykyisestä versiosta (`0.03`) kohti turvallista, standardinmukaista ja nykyaikaista sulautettujen järjestelmien HTTP/1.1-, HTTPS (OpenSSL)- ja HTTP/2-palvelinta.

Suunnitelma on jaettu kuuteen selkeään vaiheeseen (Milestones). Jokaisen vaiheen tarkat, tiedosto- ja rivikohtaiset toteutusohjeet löytyvät tiedostosta [`IMPLEMENTATION_PLAN.md`](./IMPLEMENTATION_PLAN.md).

---

## Vaihe 1: Kriittiset bugikorjaukset ja muistiturvallisuus (v0.04)
**Tavoite**: Poistaa kaatumiset, puskurin ylivuodot (buffer overflows) ja kääntäjävaroitukset sekä estää hakemistorakenteen ulkopuolisten tiedostojen luku (Path Traversal).

* [ ] **Tehtävä 1.1**: Korjaa `sprintf(stderr, ...)` -> `fprintf(stderr, ...)` ja virheellinen `time_t`-osoitinvertailu tiedostossa `source/replier.c`.
* [ ] **Tehtävä 1.2**: Korjaa `char cwd[] = ".";` puskurin ylivuoto ja lisää polun tarkistus (Path Traversal -esto `..`-poluille) tiedostossa `source/reply.c`.
* [ ] **Tehtävä 1.3**: Korjaa tiedoston rivin muokkaus (`feof()`-silmukkabugi ja tiedoston katkaisu `ftruncate`- tai uudelleenkirjoitustavalla) tiedostossa `source/reply.c`.
* [ ] **Tehtävä 1.4**: Korjaa `source/reply.h` vastaamaan `source/reply.c`:n todellisia funktioita (`html_menu`, `html_choice`) ja lisää puuttuvat otsikkotiedostot.
* [ ] **Tehtävä 1.5**: Suojaa `source/receptor.c` puskurin ylivuodoilta rajoittamalla `recv()`-lukua ja `sscanf()`-kenttien pituuksia.

---

## Vaihe 2: HTTP/1.1-standardinmukaisuus ja ominaisuuksien täydentäminen (v0.05)
**Tavoite**: Tehdä palvelimesta RFC 9112 (HTTP/1.1) -yhteensopiva ja toteuttaa puuttuvat toiminnallisuudet.

* [ ] **Tehtävä 2.1**: Päivitä `source/replier.c` käyttämään CRLF (`\r\n`) -rivinvaihtoja, RFC-muotoista `Date`-otsikkoa (`gmtime` + `strftime`) ja oikeita HTTP-tilakoodeja (`200`, `400`, `404`, `405`).
* [ ] **Tehtävä 2.2**: Toteuta HTTP-otsikoiden (`Host`, `User-Agent`, `Referer`, `Content-Length`) oikea rivi-riviltä jäsentäminen tiedostossa `source/receptor.c`.
* [ ] **Tehtävä 2.3**: Toteuta puuttuva uuden tiedoston luonti (`/choice?new=...`), johon `html_menu` jo linkittää.
* [ ] **Tehtävä 2.4**: Korjaa IPv4/IPv6-tuki (`struct sockaddr_storage`, `SO_REUSEADDR`) ja komentoriviparametrien käsittely tiedostossa `source/server.c`.

---

## Vaihe 3: Käännösjärjestelmä, Docker ja CI/CD-putki (v0.06)
**Tavoite**: Modernisoida projektin käännös, kontitus ja automaattinen laadunvarmistus.

* [ ] **Tehtävä 3.1**: Päivitä `Dockerfile` käyttämään paikallista lähdekoodia (`COPY . .`) GitHubista kloonaamisen sijaan ja korjaa `ENTRYPOINT`.
* [ ] **Tehtävä 3.2**: Korvaa vanhentunut `.travis.yml` GitHub Actions -työnkululla (`.github/workflows/ci.yml`), joka kääntää koodin `-Wall -Wextra`-lipuilla ja ajaa savutestit.

---

## Vaihe 4: OpenSSL ja HTTPS/TLS-tuki (v0.10)
**Tavoite**: Lisätä salattu HTTPS-yhteystuki OpenSSL-kirjastolla.

* [ ] **Tehtävä 4.1**: Luo verkko-I/O-abstraktiokerros (`source/net_io.h` ja `source/net_io.c`), joka eriyttää `conn_read`/`conn_write`-kutsut raaoista socket-kutsuista.
* [ ] **Tehtävä 4.2**: Lisää OpenSSL-tarkistukset `configure.ac`- ja `source/Makefile.am`-tiedostoihin (`--with-openssl`).
* [ ] **Tehtävä 4.3**: Lisää TLS-kontekstin (`SSL_CTX`) alustus, varmenteiden lataus (`--cert`, `--key`) ja `SSL_accept`-käsittely tiedostoon `source/server.c`.

---

## Vaihe 5: HTTP/2-tuki (v0.20)
**Tavoite**: Lisätä HTTP/2 (`h2`) -protokollatuki TLS/ALPN-yhteyksille.

* [ ] **Tehtävä 5.1**: Lisää ALPN-neuvottelu (`h2` ja `http/1.1`) OpenSSL-kontekstiin.
* [ ] **Tehtävä 5.2**: Integroi `libnghttp2` (`source/http2.c` ja `source/http2.h`), joka muuntaa HTTP/2-virtojen `:method`- ja `:path`-pseudo-otsikot `http_request`-rakenteeksi ja palauttaa `reply.c`:n tuottaman HTML-sivun HTTP/2 `HEADERS`- ja `DATA`-kehyksinä.

---

## Vaihe 6: Automaattitestit ja dokumentaation viimeistely (v1.0)
**Tavoite**: Varmistaa toimintavarmuus kattavilla automaattitesteillä.

* [ ] **Tehtävä 6.1**: Lisää integraatiotestiskripti (`tests/run_tests.sh`), joka testaa HTTP/1.1-, HTTPS- ja HTTP/2-pyynnöt `curl`-työkalulla.
