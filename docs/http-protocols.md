# HTTP/1.1 ja HTTP/2 -protokollat (ehttpd-arkkitehtuuridokumentaatio)

Tämä dokumentti kuvaa **HTTP/1.1**- ja **HTTP/2**-protokollien toimintaperiaatteet, niiden keskeiset erot sekä sen, miten ne liittyvät **ehttpd**-palvelimen nykyiseen toteutukseen ja tulevaan kehitykseen.

---

## 1. HTTP/1.1 (RFC 9112 / aiemmin RFC 2616)

**HTTP/1.1** (julkaistu alun perin 1997, päivitetty RFC 7230–7235 ja RFC 9112) on tekstipohjainen sovellustason protokolla, joka toimii tyypillisesti TCP-yhteyden päällä (oletusportti 80, tai TLS-salattuna portti 443).

### 1.1 Viestirakenne

Sekä HTTP/1.1-pyynnöt (Request) että vastaukset (Response) koostuvat ASCII-tekstiriveistä, jotka erotetaan toisistaan **CRLF**-merkkiparilla (`\r\n`, eli heksana `0x0D 0x0A`). Otsikko-osion (headers) ja mahdollisen rungon (body) välissä on yksi tyhjä rivi (`\r\n\r\n`).

#### Esimerkki HTTP/1.1-pyynnöstä:
```http
GET /choice?edit=/test.txt HTTP/1.1\r\n
Host: localhost:8080\r\n
User-Agent: Mozilla/5.0\r\n
Accept: text/html\r\n
Connection: close\r\n
\r\n
```

#### Esimerkki HTTP/1.1-vastauksesta:
```http
HTTP/1.1 200 OK\r\n
Date: Fri, 30 Oct 2026 12:00:00 GMT\r\n
Server: eHTTPd/0.04\r\n
Content-Length: 44\r\n
Connection: close\r\n
Content-Type: text/html; charset=utf-8\r\n
\r\n
<html><body><h1>Hei maailma!</h1></body></html>
```

### 1.2 HTTP/1.1:n keskeiset ominaisuudet

1. **Pakollinen `Host`-otsikko**:
   * Toisin kuin HTTP/1.0:ssa, HTTP/1.1 vaatii jokaisessa pyynnössä `Host`-otsikon. Jos se puuttuu, palvelimen tulee vastata tilakoodilla `400 Bad Request`.
2. **Pysyvät yhteydet (Persistent Connections / Keep-Alive)**:
   * HTTP/1.0 sulki TCP-yhteyden jokaisen pyynnön jälkeen.
   * HTTP/1.1:ssä yhteydet ovat **oletusarvoisesti pysyviä** (`Connection: keep-alive`), jolloin samaa TCP-yhteyttä voidaan käyttää useisiin peräkkäisiin pyyntöihin.
   * Jos palvelin haluaa sulkea yhteyden välittömästi vastauksen jälkeen (kuten `ehttpd` tällä hetkellä tekee), sen on lähetettävä otsikko `Connection: close` ja tiedettävä vastauksen tarkka pituus (`Content-Length`).
3. **Chunked Transfer Encoding**:
   * Mahdollistaa dynaamisen sisällön lähettämisen paloissa (`Transfer-Encoding: chunked`) silloin, kun koko vastauksen pituutta (`Content-Length`) ei tiedetä etukäteen.
4. **HTTP-metodit**:
   * `GET` (tiedon haku, ei sivuvaikutuksia), `HEAD` (vain otsikot ilman runkoa), `POST` (tiedon lähetys ja muokkaus), `PUT`, `DELETE`, `OPTIONS`.
5. **HTTP/1.1:n rajoitteet**:
   * **Head-of-Line (HOL) Blocking**: Yhden TCP-yhteyden sisällä pyynnöt on käsiteltävä tiukasti järjestyksessä. Jos yksi pyyntö on hidas, kaikki sen perässä tulevat pyynnöt samassa yhteydessä odottavat.
   * **Toisteiset tekstiotsikot**: Otsikot lähetetään pakkaamattomana tekstinä jokaisessa pyynnössä.

### 1.3 Miten `ehttpd` toteuttaa HTTP/1.1:n tällä hetkellä (ja mitä pitää korjata)

* **`source/receptor.c`**:
  * Lukee vain ensimmäisen rivin `sscanf(tcp_buffer, "%s %s HTTP/%s", ...)`-kutsulla, mutta otsikoiden (`Host`, `User-Agent`, `Referer`) varsinainen jäsentäminen puuttuu (`param` ja `value` jäävät tyhjiksi).
  * `recv()`-silmukka ei tarkista puskurin ylärajaa (`RCVBUFSIZE`), mikä mahdollistaa puskurin ylivuodon.
* **`source/replier.c`**:
  * Lähettää rivinvaihtoina pelkän `\n` standardin vaatiman `\r\n` sijaan.
  * `ctime(&now)` lisää omaan tulosteeseensa `\n`-merkin eikä ole RFC 7231 / IMF-fixdate -muodossa (`strftime(..., "%a, %d %b %Y %H:%M:%S GMT", gmtime(&now))`).
* **`source/reply.c`**:
  * Tiedostojen muokkaaminen tapahtuu `GET`-pyynnöllä (`/choice?edit=...&line=...&repl=...`), mikä rikkoo HTTP-standardin periaatetta siitä, että `GET`-pyyntö on turvallinen (idempotentti/safe) eikä muuta palvelimen tilaa. Muokkaukset tulisi siirtää `POST`-pyynnöiksi.

---

## 2. HTTP/2 (RFC 9113 / aiemmin RFC 7540)

**HTTP/2** (julkaistu 2015, päivitetty RFC 9113 vuonna 2022) säilyttää HTTP/1.1:n semantiikan (samat metodit kuten `GET`/`POST`, samat tilakoodit kuten `200`/`404`, samat otsikot ja URL-rakenteet), mutta muuttaa täysin tavan, jolla data kehystetään ja siirretään verkon yli.

### 2.1 Binääriprotokolla ja kehykset (Frames)

HTTP/2 ei ole tekstipohjainen, vaan **binääriprotokolla**. Kaikki viestintä tapahtuu pieninä binäärikehyksinä (**Frames**), joissa on kiinteä 9 tavun otsikko:
* **Length (24 bittiä)**: Kehyksen hyötykuorman pituus.
* **Type (8 bittiä)**: Kehyksen tyyppi (`DATA`, `HEADERS`, `SETTINGS`, `WINDOW_UPDATE`, `PING`, `GOAWAY`, `RST_STREAM`).
* **Flags (8 bittiä)**: Kehyskohtaiset liput (esim. `END_STREAM`, `END_HEADERS`).
* **Stream Identifier (31 bittiä)**: Virran (stream) tunniste.

### 2.2 HTTP/2:n keskeiset ominaisuudet

1. **Multipleksointi (Streams & Multiplexing)**:
   * Yksi TCP/TLS-yhteys voi sisältää kymmeniä tai satoja rinnakkaisia kaksisuuntaisia **virtoja (Streams)**.
   * Kehykset eri virroista voidaan lomittaa vapaasti. Tämä poistaa HTTP/1.1:n sovellustason Head-of-Line -jumittumisen.
2. **HPACK-otsikkopakkaus (RFC 7541)**:
   * HTTP-otsikot pakataan tehokkaasti käyttämällä staattista ja dynaamista hakutaulukkoa sekä Huffman-koodausta.
   * Esimerkiksi yleiset otsikot kuten `:method: GET` tai `:status: 200` vievät vain yhden tavun.
3. **Pseudo-otsikot (Pseudo-Header Fields)**:
   * HTTP/1.1:n pyyntörivi (`GET /index.html HTTP/1.1`) korvataan HTTP/2:ssa pseudo-otsikoilla:
     * `:method` (esim. `GET`)
     * `:scheme` (esim. `https`)
     * `:authority` (vastaa `Host`-otsikkoa)
     * `:path` (esim. `/choice?edit=/test.txt`)
   * Vastauksessa tilakoodi ilmaistaan pseudo-otsikolla `:status` (esim. `200`).
4. **Yhteyden aloitus (Connection Preface & ALPN)**:
   * Selaimet käyttävät HTTP/2:ta käytännössä vain TLS-salatun yhteyden (**HTTPS**) yli, jolloin protokolla sovitaan TLS-kättelyssä **ALPN**-laajennuksella (Application-Layer Protocol Negotiation, tunniste `"h2"`).
   * Yhteyden alussa asiakas lähettää aina 24 tavun kiinteän aloitusmerkkijonon (`PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n`) ja molemmat osapuolet vaihtavat `SETTINGS`-kehyksen.

### 2.3 Vertailu: HTTP/1.1 vs. HTTP/2

| Ominaisuus | HTTP/1.1 | HTTP/2 |
| :--- | :--- | :--- |
| **Viestimuoto** | ASCII-teksti (`\r\n`-eroteltu) | Binäärikehykset (9 tavun otsikko + hyötykuorma) |
| **Rinnakkaisuus** | 1 pyyntö kerrallaan per TCP-yhteys | Useita rinnakkaisia virtoja (Streams) yhdessä yhteydessä |
| **Otsikot** | Pakkaamaton teksti | HPACK-pakattu (Huffman + indeksitaulukot) |
| **Salaus (TLS)** | Valinnainen (`http://` tai `https://`) | Selaimissa vaatii käytännössä TLS + ALPN (`h2`) |
| **Toteutuksen monimutkaisuus C:llä** | Pieni (~100–300 riviä C-koodia) | Suuri (~3000+ riviä tai kevyt kirjasto kuten `libnghttp2`) |

### 2.4 HTTP/2-strategia `ehttpd`-projektissa

Koska `ehttpd` on suunniteltu pieniin ja sulautettuihin järjestelmiin:
1. **Vaihe A**: Korjataan ensin HTTP/1.1-toteutus täysin standardin (RFC 9112) mukaiseksi ja turvalliseksi.
2. **Vaihe B**: Lisätään **OpenSSL (TLS/HTTPS)** -tuki ja abstrakti I/O-kerros, joka toimii yhtenäisesti sekä raa'an TCP-socketin että `SSL*`-yhteyden päällä.
3. **Vaihe C**: Lisätään **HTTP/2 (`h2`)** -tuki valinnaisena käännösominaisuutena (`./configure --enable-http2`) hyödyntäen kevyttä C-kirjastoa **`libnghttp2`** sekä OpenSSL:n ALPN-neuvottelua. Näin HTTP/1.1- ja HTTP/2-pyynnöt muunnetaan samaan sisäiseen `http_request`-tietorakenteeseen ennen kuin `reply.c` käsittelee ne.
