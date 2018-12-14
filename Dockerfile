FROM ubuntu

COPY client.c server.c tcpconn.h Makefile /usr/local/src/

RUN apt-get update && \
    apt-get install -y gcc make && \
	make -C /usr/local/src && \
	mv /usr/local/src/client /usr/local/src/server /usr/local/bin && \
    apt-get purge -y gcc make && \
	apt-get autoremove -y && \
	rm -rf /var/lib/apt/lists/*

CMD echo "Syntax: client|server <options>"