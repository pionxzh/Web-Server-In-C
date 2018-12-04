#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <netdb.h>
#include <string.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>

#define PORT 5566
#define BUF_SIZE 8096

char html[] =
"HTTP/1.1 200 OK\r\n"
"ContexT-Type: text/html;charset=UTF-8\r\n\r\n"
"<!DOCTYPE html>\r\n"
"<html><head><title>Pionxzh</title></head>\r\n"
"<body><center><div style='margin-top: 20\%;font-size: 80px;'>Demo select!</div><br>\r\n"
"<img src=\"demo.jpg\"></center></body></html>\r\n";

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
    int fd_server, fd_socket;

    fd_set active_fd_set, read_fd_set;
    char buf[2048];
    int fd_img, i = 0, on = 1;

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

    FD_ZERO( &active_fd_set);
    FD_SET(fd_server, &active_fd_set);


    printf("Starting server and listening on port %d\n", PORT);
    while (1) {
        read_fd_set = active_fd_set;
        if (select(FD_SETSIZE, &read_fd_set, NULL, NULL, NULL) < 0) {
            perror("select");
            exit(EXIT_FAILURE);
        }

        for (i = 0; i < FD_SETSIZE; i++) {
            if (FD_ISSET(i, &read_fd_set)) {
                if (i == fd_server) { //connetion request on listening server
                    sin_len = sizeof(client_addr);
                    if ((fd_socket = accept(fd_server, (struct sockaddr * ) &client_addr, &sin_len)) < 0) {
                        perror("!!Connection failed.\n");
                        exit(1);
                    }

                    FD_SET(fd_socket, &active_fd_set);
                    printf("> A new client appeared!\n");
                } else {
                    memset(buf, 0, 1024);
                    read(i, buf, 1023);

                    printf("%s\n", buf);

					//other mothod is forbidden
                    if (strncmp(buf, "GET ", 4) && strncmp(buf, "get ", 4)) {
                        perror("!!Server can only accept GET method");
                        exit(1);
                    }

					//handle the request of image
                    if (!strncmp(buf, "GET /demo.jpg", 13)) {
                        printf("> sending html...\n");
                        sendFile(i, "demo.jpg", "image/jpeg");
                    } else {
                        write(i, html, sizeof(html) - 1);
                    }

                    close(i);
                    FD_CLR(i, &active_fd_set);
                }
            }
        }
    }

	return 0;
}




