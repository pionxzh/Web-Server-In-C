#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <netdb.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/socket.h>

#define PORT 5566
#define BUF_SIZE 8096

char html[] =
"HTTP/1.1 200 OK\r\n"
"Content-Type: text/html;charset=UTF-8\r\n\r\n"
"<!DOCTYPE html>\r\n"
"<html><head><title>Pionxzh</title></head>\r\n"
"<body><center><div style='margin-top: 20\%;font-size: 80px;'>Demo fork!</div><br>\r\n"
"<img src=\"demo.jpg\"></center></body></html>\r\n";

void sig_fork(int signo) {
	int stat;
	pid_t pid = waitpid(0, &stat, WNOHANG);
	return;
}

void sendFile(int fd_client, char* path, char* attr)
{
    int ret, fd_img;
    char buffer[BUF_SIZE+1];
    sprintf(buffer,"HTTP/1.1 200 OK\r\nContent-Type: %s\r\n\r\n", attr);

	if((fd_img=open(path, O_RDONLY)) == -1) {
        perror("!!Failed to open file");
        exit(1);
    }

    write(fd_client, buffer, strlen(buffer));
    while ((ret=read(fd_img, buffer, BUF_SIZE)) > 0) {
        write(fd_client, buffer, ret);
    }
    close(fd_img);
}

int main(int argc, char * argv[]) {
    struct sockaddr_in server_addr, client_addr;
    socklen_t sin_len = sizeof(client_addr);
    int fd_server, fd_client, on = 1;
    char buf[2048];

    signal(SIGCHLD, sig_fork);

    if ((fd_server = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        exit(1);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    setsockopt(fd_server, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(int));

    if (bind(fd_server, (struct sockaddr * ) &server_addr, sizeof(server_addr)) == -1) {
        perror("bind");
        close(fd_server);
        exit(1);
    }

    if (listen(fd_server, 10) == -1) {
        perror("listen");
        close(fd_server);
        exit(1);
    }

    printf("Starting server and listening on port %d\n", PORT);
    while (1) {
        fd_client = accept(fd_server, (struct sockaddr * ) &client_addr, &sin_len);

        if (fd_client == -1) {
            perror("!!Connection failed ...\n");
            continue;
        }

        if (!fork()) {
            /* child proccess */
            close(fd_server);
            memset(buf, 0, 1024);
            read(fd_client, buf, 1023);

            printf("> A new client appeared!\n");
            printf("%s\n", buf);

			//other mothod is forbidden
            if (strncmp(buf, "GET ", 4) && strncmp(buf, "get ", 4)) {
                perror("!!Server can only accept GET method");
                exit(1);
            }

			//handle the request of image
            if (!strncmp(buf, "GET /demo.jpg", 13)) {
                printf("> sending image...\n");
                sendFile(fd_client, "demo.jpg", "image/jpeg");

            } else {
                printf("> sending html...\n");
                write(fd_client, html, sizeof(html) - 1);
			}

            close(fd_client);
            printf("> Closing ...\n\n\n");
            exit(0);
        }
    }

    return 0;
}
