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

// check player number string for correctness
int checkplayernum(char *str)
{
    for (int i = 0; i < strlen(str); i++)
    {
        // printf("%c", str[i]);
        if (!isdigit(str[i]))
        {
            return 1;
        }
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
                            else
                            {
                                printf("[Server] Wrong login info.\n");
                                fflush(stdout);
                                write(client, "[Server] Wrong user/pass combination.\n[Server] Enter command: ", 62);
                            }
                        }
                        else
                        {
                            printf("[Server] Username does not exist.\n");
                            fflush(stdout);
                            write(client, "[Server] Username does not exist. Operation will not continue.\n[Server] Enter command: ", 87);
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
                else if (strcmp(received, "create-championship") == 0)
                {
                    if (!logged)
                    {
                        printf("[Server] User tried creating championship while not being logged in.\n");
                        fflush(stdout);
                        write(client, "[Server] You are not logged in. Command unavailable.\n[Server] Enter command: ", 77);
                    }
                    else
                    {
                        if (userType == 0)
                        {
                            char game_name[1000], playernum[1000], rules[1000], decider[1000];
                            printf("[Server] User intends to create championship. Sending prompts to user...\n");
                            fflush(stdout);

                            // specify game
                            write(client, "[Server] Please enter game name for championship: ", 50);
                            bzero(game_name, 1000);
                            read(client, game_name, 1000);

                            // set player number
                            write(client, "[Server] Please enter player number: ", 37);
                            bzero(playernum, 1000);
                            read(client, playernum, 1000);

                            if (checkplayernum(playernum) != 0)
                            {
                                printf("[Server] User entered wrong input for player number.\n");
                                fflush(stdout);
                                write(client, "[Server] Incorrect input for player number. Operation will not continue.\n[Server] Enter command: ", 97);
                            }

                            // set rules
                            write(client, "[Server] Please choose an option for rules: \n\t1. single elimination\n\t2. double elimination\n[Server] Option: ", 108);
                            bzero(rules, 1000);
                            read(client, rules, 1000);

                            if (strcmp(rules, "1") == 0)
                            {
                                write(client, "[Server] Single elimination rule set.\n", 38);
                            }
                            else if (strcmp(rules, "2") == 0)
                            {
                                write(client, "[Server] Double elimination rule set.\n", 38);
                            }
                            else
                            {
                                write(client, "[Server] Incorrect input for rule option. Operation will not continue.\n[Server] Enter command: ", 95);
                            }

                            // set decider
                            write(client, "[Server] Please choose an option for deciding matchups: \n\t1. Random\n\t2. Alphabetical\n\t3. By player score\n[Server] Option: ", 122);
                            bzero(decider, 1000);
                            read(client, decider, 1000);

                            if (strcmp(decider, "1") == 0)
                            {
                                write(client, "[Server] Random decider rule set.\n[Server] Championship created successfully.\n[Server] Enter command: ", 102);
                            }
                            else if (strcmp(decider, "2") == 0)
                            {
                                write(client, "[Server] Alphabetical decider rule set.\n[Server] Championship created successfully.\n[Server] Enter command: ", 108);
                            }
                            else if (strcmp(decider, "3") == 0)
                            {
                                write(client, "[Server] Player score decider rule set.\n[Server] Championship created successfully.\n[Server] Enter command: ", 108);
                            }
                            else
                            {
                                write(client, "[Server] Incorrect input for decider. Operation will not continue.\n[Server] Enter command: ", 91);
                            }

                            bzero(sql_command, 1000);
                            sprintf(sql_command, "INSERT INTO CHAMPIONSHIPS VALUES ((SELECT MAX(ID) FROM CHAMPIONSHIPS) + 1, '%s', %d, %d, %d, 0);", game_name, atoi(playernum), atoi(rules), atoi(decider));

                            rc = sqlite3_exec(db, sql_command, callback1, 0, &zErrMsg);

                            if (rc != SQLITE_OK)
                            {
                                printf("[Server] DB Error. Server will shut down.\n");
                                close(client);
                                return 1;
                            }
                            else
                            {
                                printf("[Server] Championship information saved successfully.\n");
                                fflush(stdout);
                            }
                        }
                        else
                        {
                            printf("[Server] Regular user tried using admin command.\n");
                            fflush(stdout);
                            write(client, "[Server] You do not have the permission to use this command.\n[Server] Enter command: ", 85);
                        }
                    }
                }
                else if (strcmp(received, "view-championships") == 0)
                {
                    if (!logged)
                    {
                        printf("[Server] User tried viewing championships while not being logged in.\n");
                        fflush(stdout);
                        write(client, "[Server] You are not logged in. Command unavailable.\n[Server] Enter command: ", 77);
                    }
                    else
                    {
                        char result[4000];
                        char max_id[4];
                        char championships[5000];
                        bzero(championships, 5000);
                        sprintf(championships, "ID\tGame name\t\tPlayer no.\t\tRules\t\tDecider\t\tPlayers entered\n");

                        bzero(sql_command, 4000);
                        sprintf(sql_command, "SELECT MAX(ID) FROM CHAMPIONSHIPS;");
                        bzero(max_id, 4);
                        rc = sqlite3_exec(db, sql_command, callback2, max_id, &zErrMsg);

                        int wants_to_enter = 0, quit = 0;

                        int maxid = atoi(max_id);

                        if (maxid >= 1)
                        {
                            int id = 1;

                            char *champ_id, *game_name, *playercount, *rules, *decider, *playerentered;

                            while (id <= maxid)
                            {
                                bzero(sql_command, 4000);
                                sprintf(sql_command, "SELECT ID, GAME, PLAYERNUM, RULE, DECIDER, ENTERED FROM CHAMPIONSHIPS WHERE ID=%d;", id);
                                bzero(result, 4000);
                                rc = sqlite3_exec(db, sql_command, callback2, result, &zErrMsg);

                                champ_id = strtok(result, ":");
                                game_name = strtok(NULL, ":");
                                playercount = strtok(NULL, ":");
                                rules = strtok(NULL, ":");
                                decider = strtok(NULL, ":");
                                playerentered = strtok(NULL, ":");

                                char rule[100], decide[100];
                                if (strcmp(rules, "1") == 0)
                                {
                                    strcpy(rule, "single elimination");
                                }
                                else
                                {
                                    strcpy(rule, "double elimination");
                                }

                                if (strcmp(decider, "1") == 0)
                                {
                                    strcpy(decide, "Random player pool");
                                }
                                else if (strcmp(decider, "2") == 0)
                                {
                                    strcpy(decide, "Alphabetical order");
                                }
                                else
                                {
                                    strcpy(decide, "Point based system");
                                }

                                sprintf(championships + strlen(championships), "%s\t%s\t%s\t\t%s\t\t%s\t\t%s\n", champ_id, game_name, playercount, rule, decide, playerentered);

                                id++;
                            }
                            sprintf(championships + strlen(championships), "[Server] Enter command:\n\t1. Enter championship\n\t2. Exit menu\n[Server] Option: ");
                            write(client, championships, sizeof(championships));

                            char ans[10];
                            bzero(ans, 10);
                            read(client, ans, 10);
                            if (strcmp(ans, "1") == 0)
                            {
                                write(client, "[Server] Enter ID for championship you want to enter: ", 54);
                                char enter_id[10];
                                read(client, enter_id, 10);
                                
                                if (atoi(enter_id) > 0 && atoi(enter_id) <= maxid)
                                {
                                    // TODO: update championships table and insert into entered table
                                    write(client, "[Server] Request for championship sent successfully. Check e-mail to check if you've been accepted.\n[Server] Enter command: ", 124);
                                }
                                else
                                {
                                    write(client, "[Server] Championship ID does not exist. Operation will not continue.\n[Server] Enter command: ", 94);
                                }
                            }
                            else
                            {
                                write(client, "[Server] Operation exited.\n[Server] Enter command: ", 51);
                            }
                        }
                        else
                        {
                            write(client, "[Server] No championships to show.\n[Server] Enter command: ", 59);
                        }
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
