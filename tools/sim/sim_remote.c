#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include "sim_remote.h"
#include "ui_priv.h"

static int s_listen = -1;

void sim_remote_init(int port)
{
    s_listen = socket(AF_INET, SOCK_STREAM, 0);
    int one = 1;
    setsockopt(s_listen, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    struct sockaddr_in a = { .sin_family = AF_INET, .sin_port = htons(port),
                             .sin_addr.s_addr = htonl(INADDR_LOOPBACK) };
    if (bind(s_listen, (struct sockaddr *)&a, sizeof(a)) || listen(s_listen, 4)) {
        perror("sim_remote bind");
        close(s_listen);
        s_listen = -1;
        return;
    }
    /* Non-blocking accept: the frame loop must not stall without a client */
    fcntl(s_listen, F_SETFL, fcntl(s_listen, F_GETFL, 0) | O_NONBLOCK);
    printf("sim: test commands on 127.0.0.1:%d\n", port);
}

static void send_all(const char *s, size_t len, void *arg)
{
    int fd = *(int *)arg;
    while (len) {
        ssize_t n = send(fd, s, len, 0);
        if (n <= 0) return;
        s += n;
        len -= n;
    }
}

void sim_remote_poll(void)
{
    if (s_listen < 0) return;
    int fd = accept(s_listen, NULL, NULL);
    if (fd < 0) return;
    /* The accepted socket inherits O_NONBLOCK on macOS; the reply loop wants blocking */
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) & ~O_NONBLOCK);
    struct timeval tv = { .tv_sec = 1 };
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    int nosig = 1;
    setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &nosig, sizeof(nosig));

    char line[256];
    size_t off = 0;
    while (off < sizeof(line) - 1) {
        ssize_t n = recv(fd, line + off, sizeof(line) - 1 - off, 0);
        if (n <= 0) break;
        off += n;
        if (memchr(line, '\n', off)) break;
    }
    line[off] = 0;
    ui_test_exec(line, send_all, &fd);
    close(fd);
}

void sim_remote_shutdown(void)
{
    if (s_listen >= 0) close(s_listen);
    s_listen = -1;
}
