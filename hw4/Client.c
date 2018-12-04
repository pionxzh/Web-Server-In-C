#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define MAX_BUFFER 512

void interruptHandler(int sig);

static int socketFd;

int main(int argc, char *argv[]) {
    struct sockaddr_in servaddr;
    struct hostent *host;

    if (argc != 3) {
        printf("./client [username] [host]\n");
        exit(1);
    }
    if ((host = gethostbyname(argv[2])) == NULL) {
        printf("Couldn't get host name\n");
        exit(1);
    }

    /* Sets up the socket then connects */
    bzero( &servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr = *((struct in_addr *) host->h_addr_list[0]);
    servaddr.sin_port = htons(8080);

    if ((socketFd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        printf("Couldn't create socket\n");
        exit(1);
    }

    if (connect(socketFd, (struct sockaddr *) &servaddr, sizeof(struct sockaddr)) < 0) {
        printf("Couldn't connect to server\n");
        exit(1);
    }
    /* End sets up the socket then connects */

    //Set a handler for the interrupt signal
    signal(SIGINT, interruptHandler);

    //Send username
    write(socketFd, argv[1], strlen(argv[1]) + 1);

    fd_set clientFds;
    char sendMsg[MAX_BUFFER], recvMsg[MAX_BUFFER];
    char *ptr, tmpBuffer[MAX_BUFFER];
    while (1) {
        //Reset the fd set each time since select() modifies it
        FD_ZERO( &clientFds);
        FD_SET(socketFd, &clientFds);
        FD_SET(0, &clientFds);

        if (select(FD_SETSIZE, &clientFds, NULL, NULL, NULL) != -1) { //wait for an available fd
            if (FD_ISSET(socketFd, &clientFds)) {//receive data from server
                read(socketFd, recvMsg, MAX_BUFFER - 1);
                printf("%s\n", recvMsg);
            }

            if (FD_ISSET(0, &clientFds)) { //read from keyboard (stdin) and send to server
                fgets(sendMsg, MAX_BUFFER - 1, stdin);
                sendMsg[strlen(sendMsg) - 1] = '\0';

                strcpy(tmpBuffer, sendMsg);
                ptr = strtok(tmpBuffer, " ");

                if (strcmp(sendMsg, "/exit") == 0) {
                    interruptHandler(-1); //Disconnect the client
                    exit(1);
                } else if (strcmp(ptr, "/sendFile") == 0) {
                    if ((ptr = strtok(NULL, " ")) == NULL) { //ptr is username
                        printf("/sendFile [username] [file name]\n");
                        continue;
                    }
                    if ((ptr = strtok(NULL, " ")) == NULL) { //ptr is file name
                        printf("/sendFile [username] [file name]\n");
                        continue;
                    }

                    FILE *fp = fopen(ptr, "rb");
                    if (fp == NULL) {
                        printf("File doesn't exist.\n");
                        continue;
                    }

                    //Send request
                    write(socketFd, sendMsg, strlen(sendMsg) + 1);
                    read(socketFd, recvMsg, MAX_BUFFER - 1);

                    if (strcmp(recvMsg, "FILE.") == 0) {
                        /* Connection for upload */
                        struct sockaddr_in servFileAddr;
                        bzero( &servFileAddr, sizeof(servFileAddr));
                        servFileAddr.sin_family = AF_INET;
                        servFileAddr.sin_addr = *((struct in_addr *) host->h_addr_list[0]);
                        servFileAddr.sin_port = htons(8688);

                        int socketFileFd = socket(AF_INET, SOCK_STREAM, 0);
                        connect(socketFileFd, (struct sockaddr *) &servFileAddr, sizeof(struct sockaddr));

                        int nbytes;
                        while (!feof(fp)) {
                            nbytes = fread(tmpBuffer, sizeof(char), MAX_BUFFER, fp);
                            write(socketFileFd, tmpBuffer, nbytes);
                        }

                        fclose(fp);
                        close(socketFileFd);
                        printf("Upload success.\n");
                    } else {
                        printf("Wrong user.\n");
                        continue;
                    }
                } else if (strcmp(sendMsg, "/Y") == 0) {
                    write(socketFd, sendMsg, strlen(sendMsg) + 1);
                    read(socketFd, recvMsg, MAX_BUFFER);
                    char fileName[MAX_BUFFER];
                    strcpy(fileName, "Download_");
                    strcat(fileName, recvMsg);

                    /* Connection for download */
                    struct sockaddr_in servFileAddr;
                    bzero( &servFileAddr, sizeof(servFileAddr));
                    servFileAddr.sin_family = AF_INET;
                    servFileAddr.sin_addr = *((struct in_addr *) host->h_addr_list[0]);
                    servFileAddr.sin_port = htons(8800);

                    int socketFileFd = socket(AF_INET, SOCK_STREAM, 0);
                    connect(socketFileFd, (struct sockaddr *) &servFileAddr, sizeof(struct sockaddr));

                    FILE *fp = fopen(fileName, "wb");
                    int nbytes;
                    while (nbytes = read(socketFileFd, recvMsg, MAX_BUFFER))
                        fwrite(recvMsg, sizeof(char), nbytes, fp);

                    fclose(fp);
                    close(socketFileFd);
                    printf("Download File success.\n");
                    /* End Connection for download */
                } else {
                    if (write(socketFd, sendMsg, strlen(sendMsg) + 1) == -1)
                        printf("Write failed when sending message.");
                }
            }
        } else {
            printf("select() failed\n");
            exit(1);
        }
    }
}

//Notify the server when the client exits by sending "/exit"
void interruptHandler(int sig_unused) {
    char msg[] = "/exit";
    write(socketFd, msg, strlen(msg) + 1);
    close(socketFd);
    exit(1);
}
