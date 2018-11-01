TARGET = server client

CC = gcc
CFLAGS = -pipe -Wall -Wextra -O2 -g
RM = rm -f

.PHONY: all clean

all: $(TARGET)

%: %.c tcpconn.h
	$(CC) $(CFLAGS) -o $@ $<

clean:
	$(RM) $(TARGET)
