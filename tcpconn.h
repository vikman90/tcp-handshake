#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <netdb.h>

#define DEF_PORT 1516
#define POLL_SIZE 100
#define BUF_SIZE 4096

#define print(format, ...) printf(format "\n", ##__VA_ARGS__)
#define error(format, ...) fprintf(stderr, "ERROR (%d): " format "\n", (int)getpid(), ##__VA_ARGS__)
#define debug(format, ...) if (debug_flag) fprintf(stderr, "DEBUG (%d): " format "\n", (int)getpid(), ##__VA_ARGS__)
#define verbose(format, ...) if (verbose_flag) printf(format "\n", ##__VA_ARGS__)

static const char * HC_STARTUP = "HC_STARTUP";
static const char * HC_ACK = "HC_ACK";

static void help(const char * argv0, int result) __attribute__ ((noreturn));
