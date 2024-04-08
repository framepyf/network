/*请你分别写一个客户端程序和服务器程序，客户端程序连接上服务器之后，通过敲命令和服务器进行交互，支持的交互命令包括：

pwd：显示服务器应用程序启动时的当前路径。
cd：改变服务器应用程序的当前路径。
ls：显示服务器应用程序当前路径下的文件列表。
quit：客户端进程退出，但是服务器端不能退出，第二个客户可以再次连接上服务器端。

客户端程序要求
可以指定待连接的服务器端 IP 地址和端口。
在输入一个命令之后，回车结束，之后等待服务器端将执行结果返回，客户端程序需要将结果显示在屏幕上。
样例输出如下所示。

服务器程序要求
暂时不需要考虑多个客户并发连接的情形，只考虑每次服务一个客户连接。
要把命令执行的结果返回给已连接的客户端。
服务器端不能因为客户端退出就直接退出。

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

#define DEFAULT_BACKLOG 128
#define BUFFER_SIZE 2048

#define RET_QUIT  1

// message  format
typedef struct
{
    int massageType;
    int massageLen;
    char *buff;
} privMassage_t;

enum
{
    MSG_TYPE_PWD = 1,
    MSG_TYPE_CD,
    MSG_TYPE_LS,
    MSG_TYPE_QUIT,

    MSG_TYPE_RESPONE = 100,

};

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

int cmdExec(char *cmd, char *buff, int size)
{
    FILE *fp = popen(cmd, "r");
    if (fp == NULL)
    {
        printf("Failed to run command\n");
        return -1;
    }

    if (fread(buff, size, 1, fp) < 0)
    {
        printf("Failed to read info\n");
        return -1;
    }

    return 0;
}

void handlePwdEvent(privMassage_t *pMassage, char *buff,int size)
{
    if (pMassage->massageLen == strlen("pwd") && strncmp(pMassage->buff, "pwd", strlen("pwd")) == 0)
    {
        if (getcwd(buff, size - 1) != NULL)
        {
            printf("当前工作目录为：%s\n", buff);
        }
        else
        {
            perror("获取当前工作目录失败");
            strcpy(buff, "get pwd dir error");
        }
    }
    else
    {
        strcpy(buff, "cmd error");
    }
}

void handleCdEvent(privMassage_t *pMassage, char *buff)
{
    char *path = NULL;

    if (pMassage->massageLen >= strlen("cd") && strncmp(pMassage->buff, "cd", strlen("cd")) == 0)
    {
        path = &pMassage->buff[2]; // 跳过cd
        pMassage->buff[pMassage->massageLen - 1] = '\0';

        while (*path == ' ' || *path == '\n')
            ++path; // 跳过空格 \n

        if (*path == '\0')
        {
            struct passwd *pw = getpwuid(geteuid());
            if (pw == NULL)
            {
                perror("getpwuid");
                strcpy(buff, "cd home dir error");
            }

            path = pw->pw_dir;
        }

        if (chdir(path) == -1)
        {
            perror("chdir failed");
            sprintf(buff, "%s error", pMassage->buff);
        }
        else
        {
            strcpy(buff, "cd success");
        }
    }
    else
    {
        strcpy(buff, "cmd error");
    }
}

void handleLsEvent(privMassage_t *pMassage, char *buff)
{
    if (pMassage->massageLen == strlen("ls") && strncmp(pMassage->buff, "ls", strlen("ls")) == 0)
    {
        if (cmdExec("ls", buff, BUFFER_SIZE) < 0)
        {
            strcpy(buff, "exec ls error");
        }
    }
    else
    {
        strcpy(buff, "cmd error");
    }
}

int  handleQuitEvent(privMassage_t *pMassage, char *buff)
{
    if (pMassage->massageLen == strlen("quit") && strncmp(pMassage->buff, "quit", strlen("quit")) == 0)
    {
         return 0;
    }
    else
    {
        strcpy(buff, "cmd error");
    }

    return -1;
}

int handleClientMessage(int fd, privMassage_t *pMassage)
{
    privMassage_t sendMessage;
    sendMessage.massageType = htonl(MSG_TYPE_RESPONE);
    struct iovec iovec[2];
    char buff[BUFFER_SIZE] = {0};

    switch (pMassage->massageType)
    {
    case MSG_TYPE_PWD:
        handlePwdEvent(pMassage, buff,BUFFER_SIZE);
        break;
    case MSG_TYPE_CD:
        handleCdEvent(pMassage, buff);
        break;
    case MSG_TYPE_LS:
        handleLsEvent(pMassage, buff);
        break;
    case MSG_TYPE_QUIT:
        if(0 == handleQuitEvent(pMassage, buff)){
            free(pMassage->buff);
            return RET_QUIT;
        }
        break;
    default:
        break;
    }

    free(pMassage->buff);

    sendMessage.buff = buff;
    sendMessage.massageLen = strlen(buff);

    iovec[0].iov_base = &sendMessage;
    iovec[0].iov_len = offsetof(privMassage_t, buff);
    iovec[1].iov_base = sendMessage.buff;
    iovec[1].iov_len = sendMessage.massageLen;
    sendMessage.massageLen = htonl(sendMessage.massageLen);

    if(writev(fd, iovec, 2) < 0){
        perror("write error");
    }


    return 0;
}

int createTcpListenSock(unsigned short port)
{
    struct sockaddr_in serverAddr;
    int optval = 1;
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
    {
        perror("create socket failed");
        return -1;
    }

    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0)
    {
        perror("SO_REUSEADDR set failed");
        return -1;
    }

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(fd, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0)
    {
        perror("socket bind failed");
        return -1;
    }

    if (listen(fd, DEFAULT_BACKLOG) < 0)
    {
        perror("listen failed");
        return -1;
    }
    return fd;
}

int main(int argc, char **argv)
{
    int listenFd = 0;
    int clientFd = 0;
    privMassage_t message;
    int ret = 0;

    if (argc != 2)
    {
        printf("usage:%s port\n", argv[0]);
        return -1;
    }

    listenFd = createTcpListenSock(atoi(argv[1]));
    if (listenFd <= 0)
    {
        return -1;
    }

accept_client:

    clientFd = accept(listenFd, NULL, NULL);
    if (clientFd < 0)
    {
        perror("accept failed");
        return -1;
    }

    while (1)
    {
        ret = read_one_message(clientFd, &message);
        if (ret <= 0)
        {
            goto accept_client;
        }

        if( RET_QUIT == handleClientMessage(clientFd, &message)){
              close(clientFd);
              goto accept_client;
        }
    }
}