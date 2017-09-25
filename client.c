#include "tcpconn.h"

static char * ip = "127.0.0.1";
static char * hostname;
static int debug_flag;
static int verbose_flag;
static size_t msg_size = 1024;
static struct timespec delay = { 0, 10000000 }; // 10 ms
static char * hostname;
static int sock = -1;
static in_port_t port = DEF_PORT;
static struct timeval timeout;

static void handler(int signum) {
    debug("%s received (%d)", strsignal(signum), signum);

    switch (signum) {
    case SIGINT:
        exit(EXIT_SUCCESS);
        break;

    case SIGPIPE:
        break;

    default:
        error("Unknown signal %d (%s).", signum, strsignal(signum));
    }
}

void help(const char * argv0, int result) {
    print("Syntax: %s [ -d ] [ -h ] [ -i <IP> ] [ -n <host> ] [ -p <port> ] [ -s <size> ] [ -t <ms> ] [ -v ]", argv0);
    print("");
    print("    -d          Debug mode.");
    print("    -h          This help.");
    print("    -i <IP>     IP address.");
    print("    -l <ms>     Message latency. Default: 10 ms.");
    print("    -n <host>   Hostname (instead of IP).");
    print("    -p <port>   Port number.");
    print("    -s <size>   Message size.");
    print("    -t <ms>     Sending timeout. Default: infinity.");
    print("    -v          Verbose mode (show messages).");
    exit(result);
}

static void options(int argc, char * const argv[]) {
    int c;
    long ms;
    int _port;
    int size;

    while (c = getopt(argc, argv, "dhi:l:n:p:s:t:v"), c != -1) {
        switch (c) {
        case 'd':
            debug_flag = 1;
            break;

        case 'h':
            help(argv[0], 0);

        case 'i':
            if (!optarg) {
                error("Option -%c needs an argument.", c);
                continue;
            }

            ip = optarg;
            break;

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

        case 'n':
            if (!optarg) {
                error("Option -%c needs an argument.", c);
                continue;
            }

            hostname = optarg;
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

        case 's':
            if (!optarg) {
                error("Option -%c needs an argument.", c);
                continue;
            }

            if (size = atoi(optarg), size <= 0) {
                error("Option -%c needs a positive argument.", c);
                continue;
            } else if (size > BUF_SIZE) {
                error("Option -%c requires a number lower than %d", c, BUF_SIZE);
                continue;
            }

            msg_size = size;
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

void server_connect() {
    struct sockaddr_in addr = { .sin_family = AF_INET, .sin_port = htons(port), .sin_addr = { .s_addr = 0 } };
    struct hostent * host;

    debug("server_connect()");

    if (sock >= 0) {
        debug("close()");
        close(sock);
    }

    debug("socket()");
    if (sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP), sock < 0) {
        perror("socket()");
        exit(EXIT_FAILURE);
    }

    if (timeout.tv_sec || timeout.tv_usec) {
        debug("setsockopt(SO_SNDTIMEO)");

        if (setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0) {
            perror("setsockopt(SO_SNDTIMEO)");
            exit(EXIT_FAILURE);
        }
    }

    while (1) {
        if (hostname) {
            debug("gethostbyname(%s)", hostname);

            if (host = gethostbyname(hostname), !host) {
                error("Hostname '%s' not resolved.", hostname);
                sleep(1);
                continue;
            }

            addr.sin_addr = *((struct in_addr *)host->h_addr);
            print("Trying to connect to '%s' (%s).", inet_ntoa(addr.sin_addr), hostname);
        } else {
            if (!inet_aton(ip, &addr.sin_addr)) {
                error("Invalid IP '%s'", ip);
                exit(EXIT_FAILURE);
            }

            print("Trying to connect to '%s'.", ip);
        }

        debug("connect()");
        if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
            perror("connect()");
            sleep(1);
        }

        debug("Connection stablished (not yet handshaked).");
        return;
    }
}

void server_handshake() {
    ssize_t nrecv;
    uint32_t length;
    char buffer[BUF_SIZE + 1];

    debug("server_handshake()");

    while (1) {

        length = strlen(HC_STARTUP);

        debug("send(\"%u\")", length);
        if (send(sock, (void *)&length, sizeof(length), 0) < 0) {
            perror("send(\"length(HC_STARTUP)\") [1]");
            sleep(1);
            server_connect();
            continue;
        }

        debug("send(\"%s\")", HC_STARTUP);
        if (send(sock, HC_STARTUP, length, 0) < 0) {
            perror("send(\"HC_STARTUP\") [2]");
            sleep(1);
            server_connect();
            continue;
        }

        debug("recv()");

        switch (recv(sock, (void *)&length, sizeof(length), MSG_WAITALL)) {
        case -1:
            perror("recv()");
            sleep(1);
            server_connect();
            continue;

        case 0:
            print("WARN: recv(): connection lost.");
            sleep(1);
            server_connect();
            continue;

        default:
            nrecv = recv(sock, buffer, length, MSG_WAITALL);

            if (nrecv != (ssize_t)length) {
                error("Incorrect message size from server: expecting %u, got %d", length, (int)nrecv);
                continue;
            }
        }

        buffer[nrecv] = '\0';

        if (strcmp(buffer, HC_ACK)) {
            error("recv(): expecting '%s', got '%s'", HC_ACK, buffer);
        } else {
            print("Connected to server!");
            return;
        }
    }
}

void fill_random(char * buffer, size_t msg_size) {
    static const char text[] = "abcdefghijklmnopqrstuvwxyz"
                               "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                               "0123456789"
                               "!@#$%^&*()_+-=;'[],./?";

    size_t i;

    for (i = strlen(buffer); i < msg_size; i++) {
        buffer[i] = random() % (sizeof(text) - 1);
    }
}

int main(int argc, char ** argv) {
    pid_t pid;
    int size;
    ssize_t nsend;
    uint32_t length;
    char buffer[BUF_SIZE + 1];

    options(argc, argv);
    signal(SIGINT, handler);
    signal(SIGPIPE, handler);

    server_connect();
    server_handshake();
    srandom(time(NULL));
    pid = getpid();

    while (1) {
        if (size = snprintf(buffer, sizeof(buffer), "%d: ", pid), (size_t)size > msg_size) {
            error("Composing message. Size too small?");
            return EXIT_FAILURE;
        }

        fill_random(buffer, msg_size);
        buffer[msg_size] = '\0';

        debug("send()");

        length = msg_size;

        if (nsend = send(sock, (void *)&length, sizeof(length), 0), nsend < 0) {
            if (errno == EPIPE) {
                print("Connection lost [1].");
                server_handshake();
            } else {
                perror("send(1)");
            }
        } else if ((size_t)nsend != sizeof(length)) {
            error("send(): expected %zu, got %zd", sizeof(length), nsend);
        } else {
            send(sock, buffer, msg_size, 0);
            verbose("Sent: %.80s", buffer);
            nanosleep(&delay, NULL);
        }
    }

    close(sock);
    return EXIT_SUCCESS;
}
