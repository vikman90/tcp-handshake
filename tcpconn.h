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
#define error(format, ...) fprintf(stderr, "\e[31mERROR (%d)\e[0m: " format "\n", (int)getpid(), ##__VA_ARGS__)
#define error2(format, ...) fprintf(stderr, "\e[31mERROR (%d)\e[0m: " format ": %s (%d)\n", (int)getpid(), ##__VA_ARGS__, strerror(errno), errno)
#define warn(format, ...) fprintf(stderr, "\e[33mWARN (%d)\e[0m: " format "\n", (int)getpid(), ##__VA_ARGS__)
#define warn2(format, ...) fprintf(stderr, "\e[33mWARN (%d)\e[0m: " format ": %s (%d)\n", (int)getpid(), ##__VA_ARGS__, strerror(errno), errno)
#define info(format, ...) fprintf(stderr, "\e[32mINFO (%d)\e[0m: " format "\n", (int)getpid(), ##__VA_ARGS__)
#define debug(format, ...) if (debug_flag) fprintf(stderr, "\e[34mDEBUG (%d)\e[0m: " format "\n", (int)getpid(), ##__VA_ARGS__)
#define verbose(format, ...) if (verbose_flag) printf("\e[32mINFO (%d)\e[0m: " format "\n", (int)getpid(), ##__VA_ARGS__)

static const char * HC_STARTUP = "HC_STARTUP";
static const char * HC_ACK = "HC_ACK";

static void help(const char * argv0, int result) __attribute__ ((noreturn));

typedef struct sockbuffer_t {
    char * data;
    unsigned long data_size;
    unsigned long data_len;
} sockbuffer_t;

typedef struct netbuffer_t {
    int max_fd;
    sockbuffer_t * buffers;
} netbuffer_t;

void nb_open(netbuffer_t * buffer, int sock);
int nb_close(netbuffer_t * buffer, int sock);
int nb_recv(sockbuffer_t * buffer, int sock, int (*callback)(int sock, char * data, unsigned long));
