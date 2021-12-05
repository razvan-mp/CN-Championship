#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <sqlite3.h>
#include <string.h>
#include <stdlib.h>

#define PORT 2728

//extern int errno;

char *conv_addr(struct sockaddr_in address)
{
    static char str[25];
    char port[7];

    strcpy(str, inet_ntoa(address.sin_addr));
    bzero(port, 7);
    sprintf(port, ":%d", ntohs(address.sin_port));
    strcat(str, port);
    return (str);
}

static int callback1(void *ans, int argc, char **argv, char **azColName)
{
    char query[1000];
    for (int i = 0; i < argc; i++)
    {
        strcat(query, argv[i]);
        strcat(query, " ");
    }

    return 0;
}

int main()
{
    struct sockaddr_in server;
    struct sockaddr_in from;
    fd_set readfds;
    fd_set actfds;
    struct timeval tv;
    int sd, client;
    int optval = 1;
    // int fd;
    // int nfds;
    socklen_t len = sizeof(from);
    char received[1000];
    char toSend[1000];
    char buffer[1000];

    // create socket
    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("[server] Error on socket creation().\n");
        return errno;
    }

    // set option to reuse addresses
    setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    bzero(&server, sizeof(server));

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(PORT);

    /* atasam socketul */
    if (bind(sd, (struct sockaddr *)&server, sizeof(struct sockaddr)) == -1)
    {
        perror("[server] Error on bind().\n");
        return errno;
    }

    /* punem serverul sa asculte daca vin clienti sa se conecteze */
    if (listen(sd, 5) == -1)
    {
        perror("[server] Error on listen().\n");
        return errno;
    }

    printf("[server] Waiting for connections on port %d...\n", PORT);
    fflush(stdout);

    while (true)
    {
        int client;

        if ((client = accept(sd, (struct sockaddr *)&from, &len)) < 0)
        {
            perror("[Server] Error on accept().\n");
            return 1;
        }

        int pid;
        if (-1 == (pid = fork()))
        {
            close(client);
            return 1;
        }
        else if (pid > 0)
        {
            close(client);
            sleep(1);
            continue;
        }
        else if (pid == 0)
        {
            sqlite3 *db;
            char *zErrMsg = 0;
            int rc;
            char *sql;
            // open db
            rc = sqlite3_open("userinfo.db", &db);

            if (rc)
            {
                printf("[Server] Error on opening database. Server will shut down.\n");
                return 0;
            }
            else
            {
                printf("[Server] Connected to the database successfully.\n");
                fflush(stdout);
            }

            int logged = 0, userType = -1; // 0 - admin, 1 - user
            close(sd);

            if (write(client, "[Server] Enter command: ", strlen("[Server] Enter command: ") - 1) <= 0)
            {
                perror("[Server] Error on write() to client.\n");
                exit(0);
            }
            else
            {
                printf("[Server] Message sent successfully to client.\n");
                fflush(stdout);
            }

            while (true)
            {
                printf("[Server] Waiting for message from client...\n");
                fflush(stdout);
                bzero(received, 1000);

                if (read(client, received, 1000) <= 0)
                {
                    perror("[Server] Error on read() from client.\n");
                    close(client);
                    break;
                }
                // char* command = (char*)malloc(strlen(received + 1));
                // strcpy(command, received);
                printf("[Server] Message received from client: %s\n", received);
                fflush(stdout);

                if (strncmp(received, "exit", 4) == 0)
                {
                    printf("[Server] CLIENT EXIT.\n");
                    fflush(stdout);
                    write(client, "[Server] CLIENT EXIT.\n", strlen("[Server] CLIENT EXIT.\n"));
                    close(client);
                    sqlite3_close(db);
                }
                else if (strncmp(received, "register", 8) == 0)
                {
                    char userInfo[1000] = "INSERT INTO USERINFO VALUES ('";
                    printf("[Server] Client requested register. Sending username request...\n");
                    fflush(stdout);
                    write(client, "[Server] Please provide a username: ", strlen("[Server] Please provide a username: "));
                    bzero(received, 1000);
                    read(client, received, sizeof(buffer));
                    strcat(userInfo, received);
                    strcat(userInfo, "', '");
                    write(client, "[Server] Please provide a password: ", strlen("[Server] Please provide a password: "));
                    bzero(received, 1000);
                    read(client, received, sizeof(buffer));
                    strcat(userInfo, received);
                    strcat(userInfo, "', '");
                    write(client, "[Server] Please provide your email address: ", strlen("[Server] Please provide your email address: "));
                    bzero(received, 1000);
                    read(client, received, sizeof(buffer));
                    strcat(userInfo, received);
                    strcat(userInfo, "', ");
                    write(client, "[Server] Please provide an account type (admin/ regular): ", strlen("[Server] Please provide an account type (admin/ regular): "));
                    bzero(received, 1000);
                    read(client, received, sizeof(buffer));
                    if (strncmp(received, "admin", 5) == 0)
                    {
                        write(client, "[Server] Account created successfully.\n[Server] Enter command: ", strlen("[Server] Account created successfully.\n[Server] Enter command: "));
                        strcat(userInfo, "0);");
                        rc = sqlite3_exec(db, userInfo, callback1, 0, &zErrMsg);

                        if (rc != SQLITE_OK)
                        {
                            printf("[Server] DB Error. Server will shut down.\n");
                            close(client);
                            return 1;
                        }
                        else
                        {
                            printf("[Server] User information saved successfully.\n");
                            fflush(stdout);
                        }
                    }
                    else if (strncmp(received, "regular", 7) == 0)
                    {
                        write(client, "[Server] Account created successfully.\n[Server] Enter command: ", strlen("[Server] Account created successfully.\n[Server] Enter command: "));
                        strcat(userInfo, "1);");
                        rc = sqlite3_exec(db, userInfo, callback1, 0, &zErrMsg);

                        if (rc != SQLITE_OK)
                        {
                            printf("[Server] DB Error. Server will shut down.\n");
                            close(client);
                            return 1;
                        }
                        else
                        {
                            printf("[Server] User information saved successfully.\n");
                            fflush(stdout);
                        }
                    }
                    else
                    {
                        write(client, "[Server] User account not created. Incorrect account type information. Please try again.\n[Server] Enter command: ", strlen("[Server] User account not created. Incorrect account type information. Please try again.\n[Server] Enter command: "));
                    }
                }
                else
                {
                    printf("[Server] User entered unkown command. Sending this info to client.\n");
                    fflush(stdout);
                    write(client, "[Server] Unknown command entered. Please check spelling and try again.\n[Server] Enter command: ", strlen("[Server] Unknown command entered. Please check spelling and try again.\n[Server] Enter command: "));
                }
            }
        }
    }
}
