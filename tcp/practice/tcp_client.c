/*分别写一个客户端程序和服务器程序，客户端程序连接上服务器之后，通过敲命令和服务器进行交互，支持的交互命令包括：

pwd：显示服务器应用程序启动时的当前路径。
cd：改变服务器应用程序的当前路径。
ls：显示服务器应用程序当前路径下的文件列表。
quit：客户端进程退出，但是服务器端不能退出，第二个客户可以再次连接上服务器端。

客户端程序要求
可以指定待连接的服务器端 IP 地址和端口。
在输入一个命令之后，回车结束，之后等待服务器端将执行结果返回，客户端程序需要将结果显示在屏幕上。
样例输出如下所示。

*/

#include <stdio.h>
#include <linux/in.h>
#include <sys/types.h> /* See NOTES */
#include <sys/socket.h>
#include <stdio.h>
#include <sys/time.h>
#include <unistd.h>
#include <error.h>
#include <errno.h>
#include <stddef.h>
#include <pwd.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/select.h>

/* According to earlier standards */
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>

#define EWERR -1
#define EINPUTERR -2
#define BUFFER_SIZE 256
enum
{
    MSG_TYPE_PWD = 1,
    MSG_TYPE_CD,
    MSG_TYPE_LS,
    MSG_TYPE_QUIT,

    MSG_TYPE_RESPONE = 100,

};

// message  format
typedef struct
{
    int massageType;
    int massageLen;
    char *buff;
} privMassage_t;



int readn(int fd, char *buff, int size)
{

    ssize_t num = 0;
    int wantSize = size;

    while (size > 0)
    {

        num = read(fd, buff, size);
        if (num < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                continue;
            }

            perror("read error\n");
            return -1;
        }
        else if (num == 0)
        {
            return 0;
        }

        size -= num;
    }

    return wantSize - size;
}

int read_one_message(int fd, privMassage_t *pMassage)
{
    int num = 0;

    if (pMassage == NULL)
    {
        return -1;
    }

    num = readn(fd, (char *)&pMassage->massageType, sizeof(pMassage->massageType));
    if (num != sizeof(pMassage->massageType))
    {
        return num == 0 ? 0 : -1;
    }

    pMassage->massageType = ntohl(pMassage->massageType);

    num = readn(fd, (char *)&pMassage->massageLen, sizeof(pMassage->massageLen));
    if (num != sizeof(pMassage->massageLen))
    {
        return num == 0 ? 0 : -1;
    }

    pMassage->massageLen = ntohl(pMassage->massageLen);

    pMassage->buff = (char *)malloc(pMassage->massageLen);

    if (pMassage->buff == NULL)
    {
        perror("malloc error");
        return -1;
    }

    num = readn(fd, pMassage->buff, pMassage->massageLen);
    if (num != pMassage->massageLen)
    {
        return num == 0 ? 0 : -1;
    }

    return pMassage->massageLen;
}

int createClientSockConn(char *strIp, char *strPort)
{
    struct sockaddr_in serverAddr;
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
    {
        perror("create socket error");
        return -1;
    }

    serverAddr.sin_family = PF_INET;
    serverAddr.sin_port = htons(atoi(strPort));

    if (inet_pton(AF_INET, strIp, &serverAddr.sin_addr) == 0)
    {
        perror("convert  addr error");
        return -1;
    }

    if (connect(fd, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0)
    {
        perror("connect error");
        return -1;
    }

    return fd;
}

int handleUserInput(int fd, privMassage_t *pMassage)
{
    struct iovec iovec[2];

     memset(pMassage->buff, 0, BUFFER_SIZE);
    if(fgets(pMassage->buff, BUFFER_SIZE, stdin) == NULL){
        printf("fgets %s error \n");
        return EINPUTERR;
    }


    while (*pMassage->buff == ' ')
        ++pMassage->buff;


    if (strncmp(pMassage->buff, "pwd", strlen("pwd")) == 0)
    {
        pMassage->massageType = htonl(MSG_TYPE_PWD);
        pMassage->massageLen = strlen("pwd");
    }
    else if (strncmp(pMassage->buff, "cd", strlen("cd")) == 0)
    {
        pMassage->massageType = htonl(MSG_TYPE_CD);
        pMassage->massageLen = strlen(pMassage->buff);
    }
    else if (strncmp(pMassage->buff, "ls", strlen("ls")) == 0)
    {
        pMassage->massageType = htonl(MSG_TYPE_LS);
        pMassage->massageLen = strlen("ls");
    }
    else if (strncmp(pMassage->buff, "quit", strlen("quit")) == 0)
    {
        pMassage->massageType = htonl(MSG_TYPE_QUIT);
        pMassage->massageLen = strlen("quit");
    }else{
        printf("input cmd %s error \n",pMassage->buff);
        return EINPUTERR;
    }

    iovec[0].iov_base = pMassage;
    iovec[0].iov_len = offsetof(privMassage_t, buff);
    iovec[1].iov_base = pMassage->buff;
    iovec[1].iov_len = pMassage->massageLen;
    pMassage->massageLen = htonl(pMassage->massageLen);

    if (writev(fd, iovec, 2) < 0)
    {
        perror("write error");
        return EWERR;
    }

    return EXIT_SUCCESS;
}

int main(int argc, char **argv)
{
    int clientFd = 0;
    fd_set readfds;

    fd_set cacheRfds;
    char buff[BUFFER_SIZE] = {0};
    privMassage_t messageBuf;
    int ret = 0;

    if (argc != 3)
    {
        printf("usage: %s ip port\n");
        return -1;
    }

    clientFd = createClientSockConn(argv[1], argv[2]);
    if (clientFd < 0)
    {
        return -1;
    }

    signal(SIGPIPE,SIG_IGN);

    FD_ZERO(&readfds);
    // 标准输入
    FD_SET(0, &readfds);
    FD_SET(clientFd, &readfds);

    cacheRfds = readfds;
    while (1)
    {
        readfds = cacheRfds;

        select(clientFd + 1, &readfds, NULL, NULL, NULL);

        if (FD_ISSET(0, &readfds))
        {

            messageBuf.buff = buff;

            ret = handleUserInput(clientFd, &messageBuf);
            if (ret == EWERR)
            {
               break;
            }
        }

        if (FD_ISSET(clientFd, &readfds))
        {
            memset(&messageBuf, 0, sizeof(messageBuf));
            if (read_one_message(clientFd, &messageBuf) <= 0)
            {
                printf("read_one_message exit\n");
                break;
            }

            printf("%.*s\n", messageBuf.massageLen, messageBuf.buff);
            free(messageBuf.buff);
        }
    }

    return 0;
}