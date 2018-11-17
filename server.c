#include "tcpconn.h"

static volatile int running = 1;
static int debug_flag;
static int verbose_flag;
static struct timespec delay;
static in_port_t port = DEF_PORT;
static struct timeval timeout;
static size_t acc_recv;
static netbuffer_t netbuffer;
static int nconn;
static struct timespec c_begin;
static struct timespec c_end;
static struct timespec c_diff;

static void handler(int signum) {
    debug("%s received (%d)", strsignal(signum), signum);

    switch (signum) {
    case SIGINT:
        putchar('\n');
        running = 0;
        break;

    case SIGPIPE:
        break;

    default:
        error("Unknown signal %d (%s).", signum, strsignal(signum));
    }
}

void help(const char * argv0, int result) {
    print("Syntax: %s [ -d ] [ -h ] [ -l <ms> ] [ -p <port> ] [ -v ]", argv0);
    print("");
    print("    -d          Debug mode.");
    print("    -h          This help.");
    print("    -l <ms>     Processing latency. Default: 0.");
    print("    -p <port>   Port number.");
    print("    -t <ms>     Receiving timeout. Default: infinity.");
    print("    -v          Verbose mode (show messages).");
    exit(result);
}

static void options(int argc, char * const argv[]) {
    int c;
    long ms;
    int _port;

    while (c = getopt(argc, argv, "dhl:p:t:v"), c != -1) {
        switch (c) {
        case 'd':
            debug_flag = 1;
            break;

        case 'h':
            help(argv[0], 0);

        case 'l':
            if (!optarg) {
                error("Option -%c needs an argument.", c);
                continue;
            }

            if (ms = atol(optarg), ms < 0) {
                error("Option -%c needs a positive argument.", c);
                continue;
            }

            delay.tv_sec = ms / 1000;
            delay.tv_nsec = (ms % 1000) * 1000000;
            break;

        case 'p':
            if (!optarg) {
                error("Option -%c needs an argument.", c);
                continue;
            }

            if (_port = atoi(optarg), _port <= 0) {
                error("Option -%c needs a positive argument.", c);
                continue;
            }

            port = (in_port_t)_port;
            break;

        case 't':
            if (!optarg) {
                error("Option -%c needs an argument.", c);
                continue;
            }

            if (ms = atol(optarg), ms <= 0) {
                error("Option -%c needs a positive argument.", c);
                continue;
            }

            timeout.tv_sec = ms / 1000;
            timeout.tv_usec = (ms % 1000) * 1000;
            break;

        case 'v':
            verbose_flag = 1;
            break;

        default:
            help(argv[0], 1);
        }
    }
}

int dispatch(int sock, char * data, unsigned long size) {
    uint32_t length;
    uint32_t * header;
    long nsend;
    char buffer[BUF_SIZE + 1];

    if (strncmp(data, HC_STARTUP, strlen(HC_STARTUP)) == 0) {
        debug("Client %d sent startup.", sock);

        length = strlen(HC_ACK);
        header = (uint32_t *)buffer;
        *header = length;
        memcpy(buffer + sizeof(length), HC_ACK, length);
        length += sizeof(length);

        debug("send(\"%s\")", HC_ACK);
        nsend = send(sock, buffer, length, 0);

        if (nsend != (ssize_t)length) {
            error2("send(\"HC_ACK\")");
            return -1;
        }
    } else {
        verbose("Received from %d: %.10s (%lu)", sock, data, size);

        if (delay.tv_sec || delay.tv_nsec) {
            nanosleep(&delay, NULL);
        }
    }

    return 0;
}

int main(int argc, char ** argv) {
    int sock;
    int epfd;
    int nevents;
    int i;
    long nrecv;
    struct sockaddr_in addr = { .sin_family = AF_INET, .sin_addr = { .s_addr = htonl(INADDR_ANY) } };
    struct epoll_event request = { .events = EPOLLIN };
    struct epoll_event events[POLL_SIZE] = { { .events = 0 } };

    options(argc, argv);
    signal(SIGINT, handler);
    signal(SIGPIPE, handler);

    addr.sin_port = htons(port);

    debug("socket()");
    if (sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP), sock < 0) {
        error2("socket()");
        return EXIT_FAILURE;
    }

    {
        int flag = 1;

        debug("setsockopt(REUSEADDR)");
        if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag)) < 0) {
            error2("setsockopt(REUSEADDR)");
            return EXIT_FAILURE;
        }
    }

    if (timeout.tv_sec || timeout.tv_usec) {
        debug("setsockopt(SO_RCVTIMEO)");

        if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
            error2("setsockopt(SO_RCVTIMEO)");
            return EXIT_FAILURE;
        }
    }

    debug("bind(%hu)", port);
    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        error2("bind()");
        return EXIT_FAILURE;
    }

    debug("listen(%d)", SOMAXCONN);
    if (listen(sock, SOMAXCONN) < 0) {
        error2("listen()");
        return EXIT_FAILURE;
    }

    if (epfd = epoll_create(POLL_SIZE), epfd < 0) {
        error2("epoll_create()");
        return EXIT_FAILURE;
    }

    request.data.fd = sock;

    if (epoll_ctl(epfd, EPOLL_CTL_ADD, sock, &request) < 0) {
        error2("epoll_ctl() [1]");
        return EXIT_FAILURE;
    }

    while (running) {
        nevents = epoll_wait(epfd, events, POLL_SIZE, -1);

        if (nevents < 0) {
            if (errno != EINTR) {
                error2("epoll_wait()");
            }

            continue;
        }

        debug("New events: %d", nevents);

        if (!c_begin.tv_sec) {
            clock_gettime(CLOCK_MONOTONIC, &c_begin);
        }

        for (i = 0; i < nevents; i++) {
            if (events[i].data.fd == sock) {
                debug("accept()");
                if (request.data.fd = accept(sock, NULL, NULL), request.data.fd < 0) {
                    error2("accept()");
                    continue;
                }

                nb_open(&netbuffer, request.data.fd);
                info("New connection: %d (%d)", request.data.fd, nconn);

                if (epoll_ctl(epfd, EPOLL_CTL_ADD, request.data.fd, &request) < 0) {
                    error2("epoll_ctl() [2]");
                    nb_close(&netbuffer, request.data.fd);
                }
            } else {
                nrecv = nb_recv(&netbuffer.buffers[events[i].data.fd], events[i].data.fd, dispatch);

                switch (nrecv) {
                case -1:
                    error2("recv(%d)", events[i].data.fd);
                    nb_close(&netbuffer, events[i].data.fd);
                    break;

                case 0:
                    info("Socket %d closed (%d).", events[i].data.fd, nconn);
                    nb_close(&netbuffer, events[i].data.fd);
                    break;
                }
            }
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &c_end);

    c_diff.tv_sec = c_end.tv_sec - c_begin.tv_sec;
    c_diff.tv_nsec = c_end.tv_nsec - c_begin.tv_nsec;

    if (c_diff.tv_nsec < 0) {
        c_diff.tv_sec--;
        c_diff.tv_nsec += 1000000000;
    }

    close(sock);
    close(epfd);
    info("Data received: %zu MB", acc_recv / 1000000);

    if (c_begin.tv_sec) {
        info("Time: %f sec.", (c_diff.tv_sec + (double)c_diff.tv_nsec / 1000000000));
        info("Throughput: %f Mbps.", (double)acc_recv / (c_diff.tv_sec * 1000000 + (double)c_diff.tv_nsec / 1000000) * 8);
    }

    verbose("Exiting.");
    return EXIT_SUCCESS;
}

void nb_open(netbuffer_t * buffer, int sock) {
    if (sock >= buffer->max_fd) {
        buffer->buffers = realloc(buffer->buffers, sizeof(sockbuffer_t) * (sock + 1));
        buffer->max_fd = sock;
    }

    memset(buffer->buffers + sock, 0, sizeof(sockbuffer_t));
    ++nconn;
}

int nb_close(netbuffer_t * buffer, int sock) {
    int retval = close(sock);

    if (retval) {
        error2("close(%d)", sock);
        exit(1);
    } else {
        free(buffer->buffers[sock].data);
        memset(buffer->buffers + sock, 0, sizeof(sockbuffer_t));
        --nconn;
    }

    return retval;
}

int nb_recv(sockbuffer_t * buffer, int sock, int (*callback)(int sock, char * data, unsigned long)) {
    unsigned long data_ext = buffer->data_len + BUF_SIZE;
    long recv_len;
    unsigned long i;
    unsigned long cur_offset;
    uint32_t cur_len;
    int retval = 0;

    // Extend data buffer

    if (data_ext > buffer->data_size) {
        buffer->data = realloc(buffer->data, data_ext);
        buffer->data_size = data_ext;
    }

    // Receive and append

    recv_len = recv(sock, buffer->data + buffer->data_len, BUF_SIZE, 0);

    if (recv_len <= 0) {
        return recv_len;
    }

    acc_recv += recv_len;
    buffer->data_len += recv_len;

    // Dispatch as most messages as possible

    for (i = 0; i + sizeof(uint32_t) <= buffer->data_len && !retval; i = cur_offset + cur_len) {
        cur_len = *(uint32_t *)(buffer->data + i);
        cur_offset = i + sizeof(uint32_t);

        if (cur_offset + cur_len > buffer->data_len) {
            break;
        }

        retval = callback(sock, buffer->data + cur_offset, cur_len);
        recv_len = cur_len;
    }

    // Move remaining data to data start

    if (i > 0 && i < buffer->data_len) {
        debug("i = %lu, len = %lu, size = %lu", i, buffer->data_len, buffer->data_size);
        debug("moving %d bytes", (int)(buffer->data_len - i));
        memcpy(buffer->data, buffer->data + i, buffer->data_len - i);
        buffer->data_len -= i;
    }

    return retval ? retval : recv_len;
}
