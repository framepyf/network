#include <sys/types.h>          /* See NOTES */
#include <sys/socket.h>
#include <errno.h>
#include <stdio.h>
#include <linux/in.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>


#define LISTENQ 10

#define SERV_PORT 8888

static int count;

static void sig_int(int signo) {
    printf("\nreceived %d datagrams\n", count);
    exit(0);
}

#if 0 //http 特殊字符分隔解析
size_t readline(int fd, char *buffer, size_t length) {
    char *buf_first = buffer;
    static char *buffer_pointer;
    static int nleft = 0;
    static char read_buffer[512];
    char c;

    while (--length > 0) { //先--，避免\0越界
        if (nleft <= 0) {
            int nread = recv(fd, read_buffer, sizeof(read_buffer), 0);
            if (nread < 0) {
                if (errno == EINTR) {
                    length++;
                    continue;
                }
                return -1;
            }
            if (nread == 0)
                return 0;
            buffer_pointer = read_buffer;
            nleft = nread;
        }
        c = *buffer_pointer++;
        *buffer++ = c;
        nleft--;
        if (c == '\n') {
            *buffer = '\0';
            return buffer - buf_first;
        }
    }
    return -1;
}

//每次处理一个字符，效率低
int read_line(int fd, char *buf, int size) {
    int i = 0;
    char c = '\0';
    int n;

    while ((i < size - 1) && (c != '\n')) {
        n = recv(fd, &c, 1, 0);
        if (n > 0) {
            if (c == '\r') {
                n = recv(fd, &c, 1, MSG_PEEK);
                if ((n > 0) && (c == '\n'))
                    recv(fd, &c, 1, 0);
                else
                    c = '\n';
            }
            buf[i] = c;
            i++;
        } else
            c = '\n';
    }
    buf[i] = '\0';

    return (i);
}
#endif

size_t readn(int fd, void *buffer, size_t length) {
    size_t count;
    ssize_t nread;
    char *ptr;

    ptr = buffer;
    count = length;
    while (count > 0) {
        // 如果为阻塞套接字,read有数据就会返回，不需要数据为count字节
        nread = read(fd, ptr, count);
        if (nread < 0) {
            if (errno == EINTR)
                continue;
            else
                return (-1);
        } else if (nread == 0)
            break;                /* EOF */

        count -= nread;
        ptr += nread;
    }
    return (length - count);        /* return >= 0 */
}

size_t read_message(int fd, char *buffer, size_t length) {
    u_int32_t msg_length;
    u_int32_t msg_type;
    int rc;

    rc = readn(fd, (char *) &msg_length, sizeof(u_int32_t));
    if (rc != sizeof(u_int32_t))
        return rc < 0 ? -1 : 0;
    msg_length = ntohl(msg_length);

    rc = readn(fd, (char *) &msg_type, sizeof(msg_type));
    if (rc != sizeof(u_int32_t))
        return rc < 0 ? -1 : 0;

    if (msg_length > length) {
        return -1;
    }

    rc = readn(fd, buffer, msg_length);
    if (rc != msg_length)
        return rc < 0 ? -1 : 0;
    return rc;
}

int main(int argc, char **argv) {
    int listenfd;
    listenfd = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in server_addr;
    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(SERV_PORT);

    int on = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

    int rt1 = bind(listenfd, (struct sockaddr *) &server_addr, sizeof(server_addr));
    if (rt1 < 0) {
        error(1, errno, "bind failed ");
    }

    int rt2 = listen(listenfd, LISTENQ);
    if (rt2 < 0) {
        error(1, errno, "listen failed ");
    }

    signal(SIGPIPE, SIG_IGN);

    int connfd;
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    if ((connfd = accept(listenfd, (struct sockaddr *) &client_addr, &client_len)) < 0) {
        error(1, errno, "bind failed ");
    }

    char buf[128];
    count = 0;

    while (1) {
        int n = read_message(connfd, buf, sizeof(buf));
        if (n < 0) {
            error(1, errno, "error read message");
        } else if (n == 0) {
            error(1, 0, "client closed \n");
        }
        buf[n] = 0;
        printf("received %d bytes: %s\n", n, buf);
        count++;
    }

    exit(0);

}

