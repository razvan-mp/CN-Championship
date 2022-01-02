#include "match_handler.h"
#include "mail_handler.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sqlite3.h>
#include <ctime>

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
        strcat(answer, "\n");
    }

    return 0;
}

// sqlite3 callback for UPDATE operation
static int callback3(void *ans, int argc, char **argv, char **azColName)
{
    char query[1000];

    for (int i = 0; i < argc; i++)
    {
        strcat(query, argv[i]);
        strcat(query, " ");
    }

    return 0;
}

// check player number string for correctness
int checkplayernum(char *str)
{
    for (int i = 0; i < strlen(str); i++)
        if (!isdigit(str[i]))
            return 1;

    if (atoi(str) % 4 == 1 && atoi(str) <= 0)
        return 1;

    return 0;
}

// check if id is a valid number
int check_id_number(char *str)
{
    for (int i = 0; i < strlen(str); i++)
        if (!isdigit(str[i]))
            return 1;

    return 0;
}

// add days to given date from ctime
void add_days(struct tm *date, int days_to_add)
{
    const time_t day = 86400; // number of seconds in a day
    time_t date_in_seconds = mktime(date) + (day * days_to_add);

    *date = *localtime(&date_in_seconds);
}

// calculate date for championship
char *calculate_date()
{
    int possibilities[4] = {0, 15, 30, 45};

    srand(time(NULL));

    int option = rand() % 4;
    struct tm date = {0, possibilities[option], rand() % (18 - 10 + 1) + 10};

    time_t t = time(NULL);
    struct tm now = *localtime(&t);

    int year = now.tm_year + 1901;
    int month = now.tm_mon + 1;
    int day = now.tm_mday;

    date.tm_year = year - 1900;
    date.tm_mon = month - 13;
    date.tm_mday = day;

    int rand_days = rand() % (4 - 2 + 1) + 2;

    add_days(&date, rand_days);

    char *ans = asctime(&date);
    ans[strlen(ans) - 1] = '\0';

    return ans;
}

// check if entered date is formatted correctly
int check_date(char *str)
{
    char *dd = strtok(str, ".");
    if (strlen(dd) != 2 || check_id_number(dd) == 1)
    {
        return 1;
    }

    char *mm = strtok(NULL, ".");
    if (strlen(mm) != 2 || check_id_number(mm) == 1)
    {
        return 1;
    }

    char *yyyy = strtok(NULL, ".");
    if (strlen(yyyy) != 4 || check_id_number(yyyy) == 1)
    {
        return 1;
    }

    return 0;
}

int main()
{
    struct sockaddr_in server;
    struct sockaddr_in from;
    //    fd_set readfds;
    //    fd_set actfds;
    struct timeval tv;
    int sd, client;
    int optval = 1;
    socklen_t len = sizeof(from);
    char received[1000];
    //    char toSend[1000];
    //    char buffer[1000];

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
            //            char *sql;
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
                        // set all strings to zeroes
                        bzero(sql_command, 4000);
                        bzero(user, 1000);
                        bzero(pass, 1000);
                        bzero(mail, 1000);
                        bzero(type, 1000);

                        printf("[Server] Client requested register. Sending username request...\n");
                        fflush(stdout);
                        write(client, "[Server] Please provide a username: ", 36);
                        read(client, user, 1000);

                        // check if user already exists
                        char result[4000];
                        bzero(result, 4000);
                        bzero(sql_command, 4000);
                        sprintf(sql_command, "SELECT NAME FROM USERINFO WHERE NAME='%s'", user);
                        bzero(result, 4000);
                        rc = sqlite3_exec(db, sql_command, callback2, result, &zErrMsg);

                        if (strlen(result) > 1)
                        {
                            write(client, "[Server] User already exists! Try logging in.\n[Server] Enter command: ", 70);
                        }
                        else
                        {
                            // check if user is in whitelist
                            FILE *fp;
                            char *line = NULL;
                            size_t len = 0;
                            ssize_t bytes_read;

                            fp = fopen("whitelist.txt", "r");
                            if (fp == NULL)
                                exit(EXIT_FAILURE);

                            uType = 1;
                            while ((bytes_read = getline(&line, &len, fp)) != -1)
                            {
                                if (strncmp(user, line, strlen(line) - 1) == 0)
                                    uType = 0;
                            }

                            fclose(fp);

                            write(client, "[Server] Please provide a password: ", 36);
                            read(client, pass, 1000);
                            write(client, "[Server] Please provide your email address: ", 44);
                            read(client, mail, 1000);

                            // create sql operation string
                            bzero(sql_command, 4000);
                            sprintf(sql_command, "INSERT INTO USERINFO VALUES ('%s', '%s', '%s', %d, 0);", user, pass, mail, uType);

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
                                char *typee = strtok(result, "\n");
                                typee = strtok(NULL, "\n");
                                typee = strtok(NULL, "\n");
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
                else if (strcmp(received, "add-admin") == 0)
                {
                    if (!logged)
                    {
                        printf("[Server] User tried adding admin while not being logged in.\n");
                        fflush(stdout);
                        write(client, "[Server] You are not logged in. Command unavailable.\n[Server] Enter command: ", 77);
                    }
                    else
                    {
                        write(client, "[Server] Provide username to add to whitelist: ", 47);

                        char user_to_add[1000];
                        bzero(user_to_add, 1000);
                        read(client, user_to_add, 1000);

                        // check if user is in whitelist
                        int flag = 0; // user is not in whitelist
                        FILE *fp;
                        char *line = NULL;
                        size_t len = 0;
                        ssize_t bytes_read;

                        fp = fopen("whitelist.txt", "r");
                        if (fp == NULL)
                            exit(EXIT_FAILURE);

                        while ((bytes_read = getline(&line, &len, fp)) != -1)
                        {
                            if (strncmp(user_to_add, line, strlen(line) - 1) == 0)
                                flag = 1;
                        }

                        fclose(fp);

                        if (flag == 1)
                        {
                            write(client, "[Server] Username already whitelisted.\n[Server] Enter command: ", 63);
                        }
                        else
                        {
                            fp = fopen("whitelist.txt", "a");

                            fprintf(fp, "%s", user_to_add);
                            fprintf(fp, "\n");

                            fclose(fp);

                            write(client, "[Server] Added username to whitelist successfully.\n[Server] Enter command: ", 75);
                        }
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
                            write(client, "[Server] Please choose an option for rules: \n\t1. 1 V 1\n\t2. 2 V 2\n[Server] Option: ", 82);
                            bzero(rules, 1000);
                            read(client, rules, 1000);

                            if (strcmp(rules, "1") == 0)
                            {
                                write(client, "[Server] 1 V 1 rule set.\n", 25);
                            }
                            else if (strcmp(rules, "2") == 0)
                            {
                                write(client, "[Server] 2 V 2 rule set.\n", 25);
                            }
                            else
                            {
                                write(client, "[Server] Incorrect input for rule option. Operation will not continue.\n[Server] Enter command: ", 95);
                            }

                            // set decider
                            write(client, "[Server] Please choose an option for deciding matchups: \n\t1. Random\n\t2. Alphabetical\n[Server] Option: ", 102);
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

                                champ_id = strtok(result, "\n");
                                game_name = strtok(NULL, "\n");
                                playercount = strtok(NULL, "\n");
                                rules = strtok(NULL, "\n");
                                decider = strtok(NULL, "\n");
                                playerentered = strtok(NULL, "\n");

                                char rule[100], decide[100];
                                if (strcmp(rules, "1") == 0)
                                {
                                    strcpy(rule, "1 V 1");
                                }
                                else
                                {
                                    strcpy(rule, "2 V 2");
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
                            sprintf(championships + strlen(championships), "[Server] Enter command:");
                            write(client, championships, sizeof(championships));
                        }
                        else
                        {
                            write(client, "[Server] No championships to show.\n[Server] Enter command: ", 59);
                        }
                    }
                }
                else if (strcmp(received, "enter-championship") == 0)
                {
                    if (!logged)
                    {
                        printf("[Server] User tried viewing championships while not being logged in.\n");
                        fflush(stdout);
                        write(client, "[Server] You are not logged in. Command unavailable.\n[Server] Enter command: ", 77);
                    }
                    else
                    {
                        printf("[Server] User wants to enter championship. Sending prompts...\n");
                        fflush(stdout);
                        write(client, "[Server] Please enter ID for championship you want to enter: ", 61);

                        char ans[10];
                        bzero(ans, 10);
                        read(client, ans, 10);

                        char max_id[4];
                        bzero(sql_command, 4000);
                        sprintf(sql_command, "SELECT MAX(ID) FROM CHAMPIONSHIPS;");

                        bzero(max_id, 4);
                        sqlite3_exec(db, sql_command, callback2, max_id, &zErrMsg);

                        if (check_id_number(ans) == 1)
                        {
                            write(client, "[Server] ID is not a number. Operation will not continue.\n[Server] Enter command: ", 82);
                        }
                        else
                        {
                            if (atoi(ans) <= 0 || atoi(ans) > atoi(max_id))
                            {
                                write(client, "[Server] No championship with this ID. Operation will not continue.\n[Server] Enter command: ", 92);
                            }
                            else
                            {
                                char result[1000];

                                bzero(sql_command, 4000);
                                sprintf(sql_command, "SELECT USERNAME, CHAMPID FROM SIGNEDUP WHERE CHAMPID=%s AND USERNAME='%s';", ans, user);
                                bzero(result, 1000);
                                sqlite3_exec(db, sql_command, callback2, result, &zErrMsg);

                                printf("check if user entetred: %s\n", result);

                                if (strlen(result) > 1)
                                {
                                    write(client, "[Server] Already entered this championship. Operation will not execute.\n[Server] Enter command: ", 96);
                                }
                                else
                                {

                                    bzero(sql_command, 4000);
                                    sprintf(sql_command, "SELECT PLAYERNUM, ENTERED FROM CHAMPIONSHIPS WHERE ID=%s;", ans);
                                    bzero(result, 1000);
                                    sqlite3_exec(db, sql_command, callback2, result, &zErrMsg);

                                    char *playernum, *entered;

                                    playernum = strtok(result, "\n");
                                    entered = strtok(NULL, "\n");

                                    if (atoi(entered) >= atoi(playernum))
                                    {
                                        char email_addr[1000];
                                        bzero(sql_command, 4000);
                                        sprintf(sql_command, "SELECT MAIL FROM USERINFO WHERE USERNAME='%s';", user);
                                        bzero(email_addr, 1000);
                                        sqlite3_exec(db, sql_command, callback2, email_addr, &zErrMsg);

                                        send_mail_declined(user, email_addr, ans);
                                    }
                                    else
                                    {
                                        bzero(sql_command, 4000);
                                        sprintf(sql_command, "INSERT INTO SIGNEDUP VALUES ('%s', %s)", user, ans);
                                        sqlite3_exec(db, sql_command, callback1, 0, &zErrMsg);

                                        bzero(sql_command, 4000);
                                        sprintf(sql_command, "UPDATE CHAMPIONSHIPS SET ENTERED = ENTERED + 1 WHERE ID = '%s';", ans);
                                        sqlite3_exec(db, sql_command, callback3, 0, &zErrMsg);

                                        bzero(sql_command, 4000);
                                        sprintf(sql_command, "SELECT RULE, DECIDER, ENTERED, PLAYERNUM FROM CHAMPIONSHIPS WHERE ID='%s';", ans);
                                        bzero(result, 1000);
                                        sqlite3_exec(db, sql_command, callback2, result, &zErrMsg);

                                        char *rule, *decider, *players_entered, *total_players;

                                        rule = strtok(result, "\n");
                                        decider = strtok(NULL, "\n");
                                        players_entered = strtok(NULL, "\n");
                                        total_players = strtok(NULL, "\n");

                                        int is_playernum_ok = 1;
                                        if (atoi(players_entered) == atoi(total_players))
                                            is_playernum_ok = 0;

                                        if (is_playernum_ok == 0)
                                        {
                                            char emails[4000];
                                            if (strcmp(decider, "1") == 0)
                                            {
                                                bzero(sql_command, 4000);
                                                sprintf(sql_command, "SELECT USERNAME FROM SIGNEDUP WHERE CHAMPID='%s' ORDER BY USERNAME;", ans);
                                                bzero(result, 1000);
                                                sqlite3_exec(db, sql_command, callback2, result, &zErrMsg);
                                            }
                                            else if (strcmp(decider, "2") == 0)
                                            {
                                                bzero(sql_command, 4000);
                                                sprintf(sql_command, "SELECT USERNAME FROM SIGNEDUP WHERE CHAMPID='%s' ORDER BY RANDOM();", ans);
                                                bzero(result, 1000);
                                                sqlite3_exec(db, sql_command, callback2, result, &zErrMsg);
                                            }

                                            if (strcmp(rule, "1") == 0)
                                            {
                                                char *token = strtok(result, "\n");
                                                while (token != NULL)
                                                {
                                                    char *first_user = token;
                                                    char *second_user = strtok(NULL, "\n");

                                                    printf("%s - %s\n", first_user, second_user);

                                                    // char* date = calculate_date();
                                                    bzero(sql_command, 4000);
                                                    sprintf(sql_command, "SELECT EMAIL FROM USERINFO WHERE NAME='%s' OR NAME='%s';", first_user, second_user);
                                                    bzero(emails, 4000);
                                                    sqlite3_exec(db, sql_command, callback2, emails, &zErrMsg);

                                                    char *first_mail = strtok(emails, "\n");
                                                    char *second_mail = strtok(NULL, "\n");
                                                    char *date = calculate_date();

                                                    send_mail_1v1(first_user, second_user, date, first_mail, second_mail, ans);
                                                    send_mail_1v1(second_user, first_user, date, second_mail, first_mail, ans);

                                                    token = strtok(NULL, "\n");
                                                }
                                            }
                                            else if (strcmp(rule, "2") == 0)
                                            {
                                                char *token = strtok(result, "\n");
                                                while (token != NULL)
                                                {
                                                    char *first_user = token;
                                                    char *second_user = strtok(NULL, "\n");
                                                    char *third_user = strtok(NULL, "\n");
                                                    char *fourth_user = strtok(NULL, "\n");

                                                    printf("%s, %s vs. %s, %s\n", first_user, second_user, third_user, fourth_user);

                                                    // char* date = calculate_date();
                                                    bzero(sql_command, 4000);
                                                    sprintf(sql_command, "SELECT EMAIL FROM USERINFO WHERE NAME='%s' OR NAME='%s' OR NAME='%s' OR NAME='%s';", first_user, second_user, third_user, fourth_user);
                                                    bzero(emails, 4000);
                                                    sqlite3_exec(db, sql_command, callback2, emails, &zErrMsg);

                                                    char *first_mail = strtok(emails, "\n");
                                                    char *second_mail = strtok(NULL, "\n");
                                                    char *third_mail = strtok(NULL, "\n");
                                                    char *fourth_mail = strtok(NULL, "\n");
                                                    char *date = calculate_date();

                                                    send_mail_2v2(first_user, second_user, third_user, fourth_user, first_mail, date, ans);
                                                    send_mail_2v2(second_user, first_user, third_user, fourth_user, second_mail, date, ans);
                                                    send_mail_2v2(third_user, fourth_user, first_user, second_user, third_mail, date, ans);
                                                    send_mail_2v2(fourth_user, third_user, first_user, second_user, fourth_mail, date, ans);

                                                    token = strtok(NULL, "\n");
                                                }
                                            }
                                        }
                                    }
                                    write(client, "[Server] Request for entering championship sent. You will receive an e-mail at %s with further information.\n[Server] Enter command: ", 132);
                                }
                            }
                        }
                    }
                }
                else if (strcmp(received, "change-date") == 0)
                {
                    write(client, "[Server] Enter ID for championship you want to reschedule: ", 59);
                    char ans[10];
                    char result[1000];
                    bzero(ans, 10);
                    read(client, ans, 10);

                    if (check_id_number(ans) == 0)
                    {
                        bzero(sql_command, 4000);
                        sprintf(sql_command, "SELECT CHAMPID FROM SIGNEDUP WHERE USERNAME='%s' AND CHAMPID='%s';", user, ans);
                        bzero(result, 1000);
                        sqlite3_exec(db, sql_command, callback2, result, &zErrMsg);

                        if (strlen(result) > 1)
                        {
                            write(client, "[Server] Please enter date you would like to reschedule to (format as dd.mm.yyyy): ", 83);
                            char date_candidate[100];

                            bzero(date_candidate, 100);
                            read(client, date_candidate, 100);

                            if (check_date(date_candidate) == 0)
                            {
                                write(client, "[Server] Championship rescheduled to your new selected date.\n[Server] Enter command: ", 85);
                            }
                            else
                            {
                                write(client, "[Server] Incorrect date format. Operation will not continue.\n[Server] Enter command: ", 85);
                            }
                        }
                        else
                        {
                            write(client, "[Server] You have not entered this championship. Operation will not continue.\n[Server] Enter command: ", 102);
                        }
                    }
                    else
                    {
                        write(client, "[Server] Invalid ID. Operation will not continue.\n[Server] Enter command: ", 74);
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
