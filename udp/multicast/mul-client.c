#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>

#define MULTICAST_ADDR "239.0.0.1" // 组播地址
#define PORT 3000 // 组播端口
#define BUF_SIZE 256 // 缓冲区大小

int main() {
    struct sockaddr_in addr;
    int sockfd;
    char buffer[BUF_SIZE];

    // 创建 UDP 套接字
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // 设置组播地址
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(MULTICAST_ADDR);
    addr.sin_port = htons(PORT);

    printf("Enter message to send (press Ctrl+C to exit):\n");

    while (1) {
        // 读取用户输入
        if (fgets(buffer, BUF_SIZE, stdin) == NULL) {
            break;
        }

        // 发送消息
        ssize_t num_bytes = sendto(sockfd, buffer, strlen(buffer), 0,
                                    (struct sockaddr *) &addr, sizeof(addr));
        if (num_bytes == -1) {
            perror("sendto");
            exit(EXIT_FAILURE);
        }
    }

    // 关闭套接字
    close(sockfd);

    return 0;
}

