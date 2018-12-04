#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#define MAX_BUFFER 512

/* Message queue */
typedef struct {
    char * buffer[MAX_BUFFER];
    int head, tail;
    int full, empty;
    pthread_mutex_t * mutex;
    pthread_cond_t * notFull, * notEmpty;
}
MessageQueue;

/* Client and message */
typedef struct {
    fd_set serverReadFds;
    int socketFd; // server's socket for message queue
    int clientSockets[MAX_BUFFER];
    char userNames[MAX_BUFFER][MAX_BUFFER];
    char standByFileName[MAX_BUFFER][MAX_BUFFER];
    int numClients;
    pthread_mutex_t * mutex;
    MessageQueue * queue;
}
ChatRoom;

/* Client */
typedef struct {
    int clientSocketFd;
    char userName[MAX_BUFFER];
    int index;
    ChatRoom * room;
}
Client;

void * newClientHandler(ChatRoom * room);
void * messageHandler(ChatRoom * room);
void * clientHandler(Client * client);

int main(int argc, char * argv[]) {
    struct sockaddr_in servaddr;
    int listenfd;

    /* Sets up and binds the socket to listen */
    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        printf("Create Socket failed\n");
        exit(1);
    }

    bzero( &servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(8080);

    if (bind(listenfd, (struct sockaddr * ) &servaddr, sizeof(servaddr)) == -1) {
        printf("Socket bind failed\n");
        exit(1);
    }

    if (listen(listenfd, 10) == -1) {
        printf("listen failed\n");
        exit(1);
    }
    /* End sets up and binds the socket to listen */

    /* Initializes queue */
    MessageQueue * q = (MessageQueue * ) malloc(sizeof(MessageQueue));
    if (q == NULL) {
        printf("malloc failed\n");
        exit(1);
    }

    q->empty = 1;
    q->full = q->head = q->tail = 0;

    if ((q->mutex = (pthread_mutex_t * ) malloc(sizeof(pthread_mutex_t))) == NULL) {
        printf("malloc failed\n");
        exit(1);
    }
    pthread_mutex_init(q->mutex, NULL);

    if ((q->notFull = (pthread_cond_t * ) malloc(sizeof(pthread_cond_t))) == NULL) {
        printf("malloc failed\n");
        exit(1);
    }
    pthread_cond_init(q->notFull, NULL);

    if ((q->notEmpty = (pthread_cond_t * ) malloc(sizeof(pthread_cond_t))) == NULL) {
        printf("malloc failed\n");
        exit(1);
    }
    pthread_cond_init(q->notEmpty, NULL);
    /* End initializes queue */

    /* Initialize ChatRoom */
    ChatRoom room;

    FD_ZERO( &(room.serverReadFds));
    FD_SET(listenfd, &(room.serverReadFds));

    room.socketFd = listenfd;

    memset(room.clientSockets, 0, MAX_BUFFER * sizeof(int));
    room.numClients = 0;
    for (int i = 0; i < MAX_BUFFER; i++) {
        strcpy(room.userNames[i], "\0");
        strcpy(room.standByFileName[i], "\0");
    }

    room.queue = q;

    if ((room.mutex = (pthread_mutex_t * ) malloc(sizeof(pthread_mutex_t))) == NULL) {
        printf("malloc failed\n");
        exit(1);
    }
    pthread_mutex_init(room.mutex, NULL);
    /* End initialize ChatRoom */

    //Start thread to handle new client connections
    pthread_t connectionThread;
    if ((pthread_create( &connectionThread, NULL, (void * ) &newClientHandler, &room)) == 0) {
        printf("Connection handler started\n");
    }

    //Start thread to handle messages received
    pthread_t messagesThread;
    if ((pthread_create( &messagesThread, NULL, (void * ) &messageHandler, &room)) == 0) {
        printf("Message handler started\n\n");
    }

    //Wait threads
    pthread_join(connectionThread, NULL);
    pthread_join(messagesThread, NULL);

    /* Free queue */
    pthread_mutex_destroy((room.queue)->mutex);
    pthread_cond_destroy((room.queue)->notFull);
    pthread_cond_destroy((room.queue)->notEmpty);
    free((room.queue)->mutex);
    free((room.queue)->notFull);
    free((room.queue)->notEmpty);
    free(room.queue);

    /* Free data.mutex */
    pthread_mutex_destroy(room.mutex);
    free(room.mutex);

    close(listenfd);
}

/*
 * Thread to handle new connections.
 * Adds client's fd to list of client fds and spawns a new clientHandler thread for it.
 */
void * newClientHandler(ChatRoom * room) {
    while (1) {
        int clientSocketFd = accept(room->socketFd, NULL, NULL);
        if (clientSocketFd > 0) {
            printf("Socket %d was accepted. \n", clientSocketFd);

            //Obtain lock on clients list and add new client in
            pthread_mutex_lock(room->mutex);
            if (room->numClients < MAX_BUFFER) {
                //Spawn new thread to handle client's messages
                Client * cli = (Client * ) malloc(sizeof(Client));
                cli->clientSocketFd = clientSocketFd;
                cli->room = room;

                //Add new client to list
                for (int i = 0; i < MAX_BUFFER; i++) {
                    if (!FD_ISSET(room->clientSockets[i], &(room->serverReadFds))) {
                        char buff[MAX_BUFFER];
                        read(clientSocketFd, buff, MAX_BUFFER - 1);

                        room->clientSockets[i] = clientSocketFd;
                        strcpy(room->userNames[i], buff);
                        cli->index = i;
                        strcpy(cli->userName, buff);

                        break;
                    }
                }
                FD_SET(clientSocketFd, &(room->serverReadFds));

                pthread_t clientThread;
                if ((pthread_create( &clientThread, NULL, (void * ) &clientHandler, cli)) == 0) {
                    room->numClients++;
                    printf("Socket %d joined the server.\n\n", clientSocketFd);
                } else {
                    printf("Client join failed\n\n");
                    free(cli);
                    close(clientSocketFd);
                }
            }
            pthread_mutex_unlock(room->mutex);
        }
    }
}

//The "producer" -- Listens for messages from client to add to message queue
void * clientHandler(Client * client) {
    int clientSocketFd = client->clientSocketFd;
    ChatRoom * room = client->room;
    MessageQueue * q = room->queue;

    char msgBuffer[MAX_BUFFER], sendMsg[MAX_BUFFER], tempMsg[MAX_BUFFER];
    char *ptr, tmp[MAX_BUFFER];

    while (1) {
        read(clientSocketFd, msgBuffer, MAX_BUFFER);
        strcpy(tmp, msgBuffer);
        ptr = strtok(tmp, " ");

        if (strcmp(msgBuffer, "/exit") == 0) //Remove the client from clients list and close their socket
        {
            /* Remove this client from the room */
            pthread_mutex_lock(room->mutex);

            room->clientSockets[client->index] = 0;
            close(clientSocketFd);
            room->numClients--;

            pthread_mutex_unlock(room->mutex);
            /* End remove this client from the room */

            //Broadcast this client is offline
            sprintf(sendMsg, "[System]: [%s] is offline", client->userName);
            for (int i = 0; i < MAX_BUFFER; i++)
                if (room->clientSockets[i])
                    write(room->clientSockets[i], sendMsg, strlen(sendMsg) + 1);

            printf("User [%s] on socket %d has disconnected.\n\n", client->userName,clientSocketFd);
            free(client);

            return NULL;
        } else if (strcmp(msgBuffer, "/list") == 0) //list all users in the chat room
        {
            strcpy(sendMsg, "≡");
            for (int i = 0; i < MAX_BUFFER; i++)
                if (room->clientSockets[i]) {
                    sprintf(tempMsg, "%d. %s\n", i+1, room->userNames[i]);
                    strcat(sendMsg, tempMsg);
                }
            strcat(sendMsg, "≡\n\n");
            write(clientSocketFd, sendMsg, strlen(sendMsg) + 1);
        } else if (strcmp(ptr, "/pm") == 0) //Send message to specified client
        {
            ptr = strtok(NULL, " "); // ptr is specified client's username
            for (int i = 0; i < MAX_BUFFER; i++) {
                if (strcmp(ptr, room->userNames[i]) == 0) {
                    if (i != client->index) {
                        sprintf(sendMsg, "[*][%s]: ", client->userName);

                        while ( *ptr != '\0') ptr++;
                        ptr++;
                        strcat(sendMsg, ptr);

                        write(room->clientSockets[i], sendMsg, strlen(sendMsg) + 1);
                    }

                    break;
                }
            }
        } else if (strcmp(ptr, "/sendFile") == 0) {
            ptr = strtok(NULL, " "); // ptr is specified client's username
            int i;
            for (i = 0; i < MAX_BUFFER; i++) {
                if (strcmp(ptr, room->userNames[i]) == 0) {
                    if (i != client->index) {
                        strcpy(sendMsg, "FILE.");
                        write(clientSocketFd, sendMsg, strlen(sendMsg) + 1);

                        /* Connection for file upload */
                        struct sockaddr_in servFileAddr;
                        int listenFileFd;

                        listenFileFd = socket(AF_INET, SOCK_STREAM, 0);
                        bzero( &servFileAddr, sizeof(servFileAddr));

                        servFileAddr.sin_family = AF_INET;
                        servFileAddr.sin_addr.s_addr = htonl(INADDR_ANY);
                        servFileAddr.sin_port = htons(8688);

                        bind(listenFileFd, (struct sockaddr * ) &servFileAddr, sizeof(servFileAddr));
                        listen(listenFileFd, 1);


                        int clientFileFd = accept(listenFileFd, NULL, NULL);
                        printf("Server waiting for uploading");
                        //File Upload
                        ptr = strtok(NULL, " "); // ptr is file name
                        strcpy(msgBuffer, "Server_");
                        strcat(msgBuffer, ptr);
                        FILE * fp = fopen(msgBuffer, "wb");
                        int nbytes;

                        while (1) {
                            if ((nbytes = read(clientFileFd, tmp, MAX_BUFFER)) == 0)
                                break;

                            fwrite(tmp, sizeof(char), nbytes, fp);
                        }
                        fclose(fp);

                        close(clientFileFd);
                        close(listenFileFd);
                        strcpy(room->standByFileName[i], msgBuffer);
                        /* End connection for file upload */

                        sprintf(sendMsg, "[System]: [%s] want to send a file to you, accept? (/Y or /N)", client->userName);
                        write(room->clientSockets[i], sendMsg, strlen(sendMsg) + 1);
                    } else {
                        strcpy(sendMsg, "Invalid.");
                        write(clientSocketFd, sendMsg, strlen(sendMsg) + 1);
                    }

                    break;
                }
            }

            if (i == MAX_BUFFER) {
                strcpy(sendMsg, "Invalid.");
                write(clientSocketFd, sendMsg, strlen(sendMsg) + 1);
                continue;
            }
        } else if ((strcmp(msgBuffer, "/Y") == 0) || (strcmp(msgBuffer, "/N") == 0)) {
            char fileName[MAX_BUFFER];
            strcpy(fileName, room->standByFileName[client->index]);

            if (msgBuffer[1] == 'Y') {
                /* Connection for file download */
                struct sockaddr_in servFileAddr;
                int listenFileFd;

                listenFileFd = socket(AF_INET, SOCK_STREAM, 0);
                bzero( &servFileAddr, sizeof(servFileAddr));

                servFileAddr.sin_family = AF_INET;
                servFileAddr.sin_addr.s_addr = htonl(INADDR_ANY);
                servFileAddr.sin_port = htons(8800);

                bind(listenFileFd, (struct sockaddr * ) &servFileAddr, sizeof(servFileAddr));
                listen(listenFileFd, 1);

                write(clientSocketFd, fileName, strlen(fileName) + 1);

                int clientFileFd = accept(listenFileFd, NULL, NULL);

                FILE * fp = fopen(fileName, "rb");
                int nbytes;
                while (!feof(fp)) {
                    nbytes = fread(msgBuffer, sizeof(char), MAX_BUFFER, fp);
                    write(clientFileFd, msgBuffer, nbytes);
                }

                fclose(fp);
                close(clientFileFd);
                close(listenFileFd);
                /* End connection for file download */
            }

            remove(fileName);
            strcpy(room->standByFileName[client->index], "\0");
        } else {
            //Wait for queue to not be full before pushing message
            while (q->full)
                pthread_cond_wait(q->notFull, q->mutex);

            //Build the message
            strcpy(sendMsg, "\0");
            sprintf(tempMsg, "[%s]: %s", client->userName, msgBuffer);
            strcpy(sendMsg, tempMsg);

            ptr = (char * ) malloc(strlen(sendMsg) * sizeof(char) + sizeof(char));
            strcpy(ptr, sendMsg);

            /* Push message to queue */
            pthread_mutex_lock(q->mutex);
            printf("[PUSHING]:   \"%s\"\n", ptr);

            q->buffer[q->tail] = ptr;
            q->tail++;
            if (q->tail == MAX_BUFFER)
                q->tail = 0;
            if (q->tail == q->head)
                q->full = 1;
            q->empty = 0;

            pthread_mutex_unlock(q->mutex);
            /* End Push message to queue */

            pthread_cond_signal(q->notEmpty);
        }
    }
}

//The "consumer" -- waits for the queue to have messages then takes them out and broadcasts to clients
void * messageHandler(ChatRoom * room) {
    MessageQueue * q = room->queue;
    int * clientSockets = room->clientSockets;

    while (1) {
        /* Pop message from queue when not empty */
        pthread_mutex_lock(q->mutex);

        while (q->empty)
            pthread_cond_wait(q->notEmpty, q->mutex);

        printf("[POPPING]:    message from the queue.\n");

        char * msg = q->buffer[q->head];
        q->head++;
        if (q->head == MAX_BUFFER)
            q->head = 0;
        if (q->head == q->tail)
            q->empty = 1;
        q->full = 0;

        pthread_mutex_unlock(q->mutex);
        pthread_cond_signal(q->notFull);
        /* End pop message from queue */

        //Broadcast message to all connected clients
        printf("[Broadcast]: \"%s\"\n\n", msg);
        for (int i = 0; i < MAX_BUFFER; i++) {
            int socket = clientSockets[i];

            if (socket != 0)
                if ((write(socket, msg, strlen(msg) + 1) == -1))
                    printf("Socket write failed\n");
        }
        free(msg);
    }
}
