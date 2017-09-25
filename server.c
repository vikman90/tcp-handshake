#include "tcpconn.h"

static volatile int running = 1;
static int debug_flag;
static int verbose_flag;
static struct timespec delay;
static in_port_t port = DEF_PORT;
static struct timeval timeout;

static void handler(int signum) {
    debug("%s received (%d)", strsignal(signum), signum);

    switch (signum) {
    case SIGINT:
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

            if (ms = atol(optarg), ms <= 0) {
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

int main(int argc, char ** argv) {
    int sock;
    int epfd;
    int nevents;
    int i;
    int nconn = 0;
    ssize_t nrecv;
    uint32_t length;
    char buffer[BUF_SIZE + 1];
    struct sockaddr_in addr = { .sin_family = AF_INET, .sin_addr = { .s_addr = htonl(INADDR_ANY) } };
    struct epoll_event request = { .events = EPOLLIN };
    struct epoll_event events[POLL_SIZE] = { { .events = 0 } };

    options(argc, argv);
    signal(SIGINT, handler);
    signal(SIGPIPE, handler);

    addr.sin_port = htons(port);

    debug("socket()");
    if (sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP), sock < 0) {
        perror("socket()");
        return EXIT_FAILURE;
    }

    {
        int flag = 1;

        debug("setsockopt(REUSEADDR)");
        if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag)) < 0) {
            perror("setsockopt(REUSEADDR)");
            return EXIT_FAILURE;
        }
    }

    if (timeout.tv_sec || timeout.tv_usec) {
        debug("setsockopt(SO_RCVTIMEO)");

        if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
            perror("setsockopt(SO_RCVTIMEO)");
            return EXIT_FAILURE;
        }
    }

    debug("bind(%hu)", port);
    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind()");
        return EXIT_FAILURE;
    }

    debug("listen(%d)", SOMAXCONN);
    if (listen(sock, SOMAXCONN) < 0) {
        perror("listen()");
        return EXIT_FAILURE;
    }

    if (epfd = epoll_create(POLL_SIZE), epfd < 0) {
        perror("epoll_create()");
        return EXIT_FAILURE;
    }

    request.data.fd = sock;

    if (epoll_ctl(epfd, EPOLL_CTL_ADD, sock, &request) < 0) {
        perror("epoll_ctl() [1]");
        return EXIT_FAILURE;
    }

    while (running) {
        debug("epoll_wait()");
        if (nevents = epoll_wait(epfd, events, POLL_SIZE, -1), nevents < 0) {
            if (errno != EINTR) {
                perror("epoll_wait()");
            }

            continue;
        }

        debug("New events: %d", nevents);

        for (i = 0; i < nevents; i++) {
            if (events[i].data.fd == sock) {
                debug("accept()");
                if (request.data.fd = accept(sock, NULL, NULL), request.data.fd < 0) {
                    perror("accept()");
                    continue;
                }

                print("New connection (%d).", ++nconn);

                if (epoll_ctl(epfd, EPOLL_CTL_ADD, request.data.fd, &request) < 0) {
                    perror("epoll_ctl() [2]");
                    close(request.data.fd);
                    nconn--;
                }
            } else {
                debug("recv()");

                switch (recv(events[i].data.fd, (void *)&length, sizeof(length), MSG_WAITALL)) {
                case -1:
                    perror("recv()");
                    close(events[i].data.fd);
                    nconn--;
                    break;

                case 0:
                    print("Socket %d closed (%d).", events[i].data.fd, --nconn);
                    close(events[i].data.fd);
                    nconn--;
                    break;

                default:
                    nrecv = recv(events[i].data.fd, buffer, length, MSG_WAITALL);

                    if (nrecv != (ssize_t)length) {
                        error("Incorrect message size from client %d: expecting %u, got %d", events[i].data.fd, length, (int)nrecv);
                        close(events[i].data.fd);
                        nconn--;
                        break;
                    }

                    buffer[nrecv] = '\0';

                    if (strcmp(buffer, HC_STARTUP) == 0) {
                        debug("Client %d sent startup.", events[i].data.fd);

                        length = strlen(HC_ACK);
                        debug("send(\"%s\")", HC_ACK);

                        if (send(events[i].data.fd, (void *)&length, sizeof(length), 0) < 0 || send(events[i].data.fd, HC_ACK, length, 0) < 0) {
                            perror("send(\"HC_ACK\")");
                            close(events[i].data.fd);
                            nconn--;
                        }
                    } else {
                        verbose("Received from %d: %.10s (%zd)", events[i].data.fd, buffer, nrecv);
                        nanosleep(&delay, NULL);
                    }
                }
            }
        }
    }

    close(sock);
    close(epfd);
    verbose("Exiting.");
    return EXIT_SUCCESS;
}
