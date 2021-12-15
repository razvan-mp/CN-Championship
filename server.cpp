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

// convert address (from the professor's course)
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

// sqlite3 callback for INSERT operation
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

// sqlite3 callback for SELECT operation
static int callback2(void *ans, int argc, char **argv, char **azColName)
{
    char *answer = (char *)ans;

    for (int i = 0; i < argc; i++)
    {
        strcat(answer, argv[i]);
        strcat(answer, ":");
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

    // bind socket
    if (bind(sd, (struct sockaddr *)&server, sizeof(struct sockaddr)) == -1)
    {
        perror("[Server] Error on bind().\n");
        return errno;
    }

    // listen for new connections
    if (listen(sd, 5) == -1)
    {
        perror("[Server] Error on listen().\n");
        return errno;
    }
    // let server watcher know what's going on
    printf("[Server] Waiting for connections on port %d...\n", PORT);
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
        // if fork unsuccessful and client wasn't created
        if (-1 == (pid = fork()))
        {
            close(client);
            return 1;
        }
        // if client exited or server ran into error
        else if (pid > 0)
        {
            close(client);
            sleep(1);
            continue;
        }
        // child for every new client
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
            char sql_command[4000], user[1000], pass[1000], mail[1000], type[1000];
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

                printf("[Server] Message received from client: %s\n", received);
                fflush(stdout);

                // check for exit from client and close it if check passes
                if (strcmp(received, "exit") == 0)
                {
                    printf("[Server] CLIENT EXIT.\n");
                    fflush(stdout);
                    write(client, "[Server] CLIENT EXIT.\n", 22);
                    close(client);
                    sqlite3_close(db);
                }
                // register user
                else if (strcmp(received, "register") == 0)
                {
                    if (!logged)
                    {
                        int uType;
                        // set all strings to 0
                        bzero(sql_command, 4000);
                        bzero(user, 1000);
                        bzero(pass, 1000);
                        bzero(mail, 1000);
                        bzero(type, 1000);

                        printf("[Server] Client requested register. Sending username request...\n");
                        fflush(stdout);
                        write(client, "[Server] Please provide a username: ", 36);
                        read(client, user, 1000);
                        write(client, "[Server] Please provide a password: ", 36);
                        read(client, pass, 1000);
                        write(client, "[Server] Please provide your email address: ", 44);
                        read(client, mail, 1000);
                        write(client, "[Server] Please provide an account type (admin/ regular): ", 58);
                        read(client, type, 1000);
                        if (strcmp(type, "admin") == 0)
                        {
                            uType = 0;
                            // create sql operation string
                            sprintf(sql_command, "INSERT INTO USERINFO VALUES ('%s', '%s', '%s', %d);", user, pass, mail, uType);

                            write(client, "[Server] Account created successfully.\n[Server] Enter command: ", 63);
                            rc = sqlite3_exec(db, sql_command, callback1, 0, &zErrMsg);

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
                        else if (strcmp(received, "regular") == 0)
                        {
                            uType = 1;
                            // create sql operation string
                            sprintf(sql_command, "INSERT INTO USERINFO VALUES ('%s', '%s', '%s', %d);", user, pass, mail, uType);

                            write(client, "[Server] Account created successfully.\n[Server] Enter command: ", 63);
                            rc = sqlite3_exec(db, sql_command, callback1, 0, &zErrMsg);

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
                            write(client, "[Server] User account not created. Incorrect account type information. Please try again.\n[Server] Enter command: ", 113);
                        }
                    }
                    else
                    {
                        printf("[Server] User entered 'register' while logged in.\n");
                        fflush(stdout);
                        write(client, "[Server] Log out to use this command.\n[Server] Enter command: ", 63);
                    }
                }
                else if (strcmp(received, "login") == 0)
                {
                    if (!logged)
                    {
                        char result[4000];
                        printf("[Server] User requested login. Sending user and pass prompt...\n");
                        fflush(stdout);

                        write(client, "[Server] Enter your username: ", 30);
                        bzero(user, 1000);
                        read(client, user, 1000);

                        bzero(sql_command, 4000);
                        sprintf(sql_command, "SELECT NAME FROM USERINFO WHERE NAME='%s'", user);
                        bzero(result, 4000);
                        rc = sqlite3_exec(db, sql_command, callback2, result, &zErrMsg);

                        if (strlen(result) > 1)
                        {
                            printf("[Server] username exists. Sending pass prompt...\n");
                            fflush(stdout);
                            write(client, "[Server] Enter your password: ", 30);

                            bzero(pass, 1000);
                            read(client, pass, 1000);

                            bzero(sql_command, 4000);
                            sprintf(sql_command, "SELECT NAME, PASS, TYPE FROM USERINFO WHERE NAME='%s' AND PASS='%s'", user, pass);
                            bzero(result, 4000);
                            rc = sqlite3_exec(db, sql_command, callback2, result, &zErrMsg);
                            
                            if (strlen(result) > 1)
                            {
                                char *typee = strtok(result, ":");
                                typee = strtok(NULL, ":");
                                typee = strtok(NULL, ":");
                                logged = 1;
                                userType = atoi(typee);
                                printf("[Server] User/pass combo correct.\n");
                                fflush(stdout);

                                write(client, "[Server] Logged in successfully.\n[Server] Enter command: ", 57);
                            }
                        }
                    }
                    else
                    {
                        printf("[Server] User entered 'login' while logged in.\n");
                        fflush(stdout);
                        write(client, "[Server] Log out to use this command.\n[Server] Enter command: ", 63);
                    }
                }
                else if (strcmp(received, "logout") == 0)
                {
                    if (!logged)
                    {
                        printf("[Server] User tried logging out while not being logged in.\n");
                        fflush(stdout);
                        write(client, "[Server] You are not logged in. Command unavailable.\n[Server] Enter command: ", 77);
                    }
                    else
                    {
                        logged = 0;
                        userType = -1;

                        printf("[Server] User logged out.\n");
                        fflush(stdout);
                        write(client, "[Server] Logged out successfully.\n[Server] Enter command: ", 58);
                    }
                }
                else
                // end of control sequence
                // if reached, the command is considered unknown and user is informed
                {
                    printf("[Server] User entered unkown command. Sending this info to client.\n");
                    fflush(stdout);
                    write(client, "[Server] Unknown command entered. Please check spelling and try again.\n[Server] Enter command: ", 95);
                }
            }
        }
    }
}
