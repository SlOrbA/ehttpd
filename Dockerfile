FROM alpine AS build

RUN apk add alpine-sdk automake autoconf libtool git

RUN git clone https://github.com/SlOrbA/ehttpd.git

WORKDIR /ehttpd

RUN automake

RUN ./configure

RUN make

RUN make install

FROM alpine AS final

COPY --from=build /usr/local/bin/ehttpd .

EXPOSE 8080

ENTRYPOINT ["ehttpd"]
