TARGET = server client

CC = gcc
#CFLAGS = -pipe -Wall -Wextra -no-pie -pg -g
CFLAGS = -pipe -Wall -Wextra -O2 -pthread
RM = rm -f

.PHONY: all clean

all: $(TARGET)

%: %.c tcpconn.h
	$(CC) $(CFLAGS) -o $@ $<

clean:
	$(RM) $(TARGET)
