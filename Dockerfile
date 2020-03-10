FROM alpine AS build

COPY * /

RUN apk add automake autoconf gcc git

RUN git clone https://github.com/SlOrbA/ehttpd.git

WORKDIR /ehttpd

RUN automake

RUN configure

RUN make

EXPOSE 8080
