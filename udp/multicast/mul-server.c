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
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(PORT);

    // 绑定套接字
    if (bind(sockfd, (struct sockaddr *) &addr, sizeof(addr)) == -1) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    //接收组播需加入组播组
    struct ip_mreq mreq;
    mreq.imr_multiaddr.s_addr = inet_addr(MULTICAST_ADDR);
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    if (setsockopt(sockfd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) == -1) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    printf("Server listening...\n");

    while (1) {
        // 接收消息
        ssize_t num_bytes = recvfrom(sockfd, buffer, BUF_SIZE, 0, NULL, NULL);
        if (num_bytes == -1) {
            perror("recvfrom");
            exit(EXIT_FAILURE);
        }
        buffer[num_bytes] = '\0'; // 添加字符串结束符

        printf("Received message: %s\n", buffer);
    }

    // 关闭套接字
    close(sockfd);

    return 0;
}

