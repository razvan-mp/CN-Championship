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
#include <string>
#include <regex>

#define PORT 2728

// sqlite3 callback for SELECT operation
static int callback1(void *ans, int argc, char **argv, char **azColName)
{
    char *answer = (char *)ans;

    for (int i = 0; i < argc; i++)
    {
        strcat(answer, argv[i]);
        strcat(answer, "\n");
    }

    return 0;
}

// check if number is power of two
bool power_of_two(int n)
{
    return n && (!(n & (n - 1)));
}

// check if number is power of four
bool power_of_four(int n)
{
    return ((n & (n - 1)) == 0) && !(n & 0xAAAAAAAA);
}

// check player number string for correctness
int checkplayernum(char *str, int decider)
{
    for (int i = 0; i < strlen(str); i++)
        if (!isdigit(str[i]))
            return 1;

    if (atoi(str) % 4 == 1 && atoi(str) <= 0)
        return 1;

    if (decider == 1)
    {
        if (power_of_two(atoi(str)) == false)
            return 1;
    }
    else if (decider == 2)
    {
        if (power_of_four(atoi(str)) == false)
            return 1;
    }
    else
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

// check if username contains special characters
bool check_username(char *user)
{
    const char *invalid_chars = "~!@#$%^&*()_+`=-|}{:\"<>?/,.;'[]\\";
    char *c = user;

    while (*c)
    {
        if (strchr(invalid_chars, *c))
            return false;

        c++;
    }
    return true;
}

// check if an e-mail address is valid or not
bool check_email(char *email)
{
    std::string mail(email);

    const std::regex pattern("(\\w+)(\\.|_)?(\\w*)@(yahoo\\.com|gmail\\.com|info\\.uaic\\.ro)+");

    return std::regex_match(mail, pattern);
}

int main()
{
    struct sockaddr_in server;
    struct sockaddr_in from;
    struct timeval tv;
    int sd, client;
    int optval = 1;
    socklen_t len = sizeof(from);
    char received[1000];

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

            if (write(client, "[Server] Enter 'help' for a list of commands or enter command: ", strlen("[Server] Enter 'help' for a list of commands or enter command: ") - 1) <= 0)
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
                else if (strcmp(received, "help") == 0)
                {
                    if (!logged)
                    {
                        write(client, "[Server] Available commands:\n\t1. help\n\t2. login\n\t3. register\n\t4. exit\n[Server] Enter command: ", 94);
                    }

                    if (logged && userType == 0)
                    {
                        write(client, "[Server] Available commands:\n\t1. help\n\t2. create-championship\n\t3. view-championships\n\t4. enter-championship\n\t5. change-date\n\t6. add-admin\n\t7. logout\n\t8. exit\n[Server] Enter command: ", 182);
                    }

                    if (logged && userType == 1)
                    {
                        write(client, "[Server] Available commands:\n\t1. help\n\t2. view-championships\n\t3. enter-championship\n\t4. change-date\n\t5. logout\n\t6. exit\n[Server] Enter command: ", 144);
                    }
                }
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
                        rc = sqlite3_exec(db, sql_command, callback1, result, &zErrMsg);

                        if (strlen(result) > 1)
                        {
                            write(client, "[Server] User already exists! Try logging in.\n[Server] Enter command: ", 70);
                        }
                        else if (check_username(user) == false)
                        {
                            write(client, "[Server] Username cannot contain special characters! Operation will not continue.\n[Server] Enter command: ", 106);
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

                            write(client, "[Server] Account created successfully.\n[Server] Enter 'help' for a list of commands or enter command: ", 102);
                            rc = sqlite3_exec(db, sql_command, NULL, 0, &zErrMsg);

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
                        rc = sqlite3_exec(db, sql_command, callback1, result, &zErrMsg);

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
                            rc = sqlite3_exec(db, sql_command, callback1, result, &zErrMsg);

                            if (strlen(result) > 1)
                            {
                                char *typee = strtok(result, "\n");
                                typee = strtok(NULL, "\n");
                                typee = strtok(NULL, "\n");
                                logged = 1;
                                userType = atoi(typee);
                                printf("[Server] User/pass combo correct.\n");
                                fflush(stdout);

                                write(client, "[Server] Logged in successfully.\n[Server] Enter 'help' for a list of commands or enter command: ", 96);
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

                            // set rules
                            write(client, "[Server] Please choose an option for rules: \n\t1. 1 V 1\n\t2. 2 V 2\n[Server] Option: ", 82);
                            bzero(rules, 1000);
                            read(client, rules, 1000);

                            int flag_rules = -1;

                            if (strcmp(rules, "1") == 0)
                            {
                                flag_rules = 1;
                                write(client, "[Server] 1 V 1 rule set.\n", 25);
                            }
                            else if (strcmp(rules, "2") == 0)
                            {
                                flag_rules = 1;
                                write(client, "[Server] 2 V 2 rule set.\n", 25);
                            }
                            else
                            {
                                flag_rules = 1;
                            }

                            // set decider
                            write(client, "[Server] Please choose an option for deciding matchups: \n\t1. Random\n\t2. Alphabetical\n[Server] Option: ", 102);
                            bzero(decider, 1000);
                            read(client, decider, 1000);

                            int flag_decider = -1;

                            if (strcmp(decider, "1") == 0)
                            {
                                flag_decider = 0;
                                write(client, "[Server] Random decider rule set.\n", 34);
                            }
                            else if (strcmp(decider, "2") == 0)
                            {
                                flag_decider = 0;
                                write(client, "[Server] Alphabetical decider rule set.\n", 40);
                            }
                            else
                            {
                                flag_decider = 1;
                            }

                            // set player number
                            write(client, "[Server] Please enter player number: ", 37);
                            bzero(playernum, 1000);
                            read(client, playernum, 1000);

                            write(client, "[Server] Player number set. Trying to create championship...\n", 61);

                            if ((atoi(decider) == 1 || atoi(decider) == 2) && (atoi(rules) == 1 || atoi(rules) == 2) && checkplayernum(playernum, atoi(decider) == 0))
                            {
                                bzero(sql_command, 1000);
                                sprintf(sql_command, "INSERT INTO CHAMPIONSHIPS VALUES ((SELECT MAX(ID) FROM CHAMPIONSHIPS) + 1, '%s', %d, %d, %d, 0);", game_name, atoi(playernum), atoi(rules), atoi(decider));
                                rc = sqlite3_exec(db, sql_command, NULL, 0, &zErrMsg);
                            }
                            else
                            {
                                write(client, "[Server] Wrong inputs. Championship not created.\n[Server] Enter command: ", 73);
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
                        rc = sqlite3_exec(db, sql_command, callback1, max_id, &zErrMsg);

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
                                rc = sqlite3_exec(db, sql_command, callback1, result, &zErrMsg);

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
                        sqlite3_exec(db, sql_command, callback1, max_id, &zErrMsg);

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
                                sqlite3_exec(db, sql_command, callback1, result, &zErrMsg);

                                if (strlen(result) > 1)
                                {
                                    write(client, "[Server] Already entered this championship. Operation will not execute.\n[Server] Enter command: ", 96);
                                }
                                else
                                {

                                    bzero(sql_command, 4000);
                                    sprintf(sql_command, "SELECT PLAYERNUM, ENTERED FROM CHAMPIONSHIPS WHERE ID=%s;", ans);
                                    bzero(result, 1000);
                                    sqlite3_exec(db, sql_command, callback1, result, &zErrMsg);

                                    char *playernum, *entered;

                                    playernum = strtok(result, "\n");
                                    entered = strtok(NULL, "\n");

                                    if (atoi(entered) >= atoi(playernum))
                                    {
                                        char email_addr[1000];
                                        bzero(sql_command, 4000);
                                        sprintf(sql_command, "SELECT MAIL FROM USERINFO WHERE USERNAME='%s';", user);
                                        bzero(email_addr, 1000);
                                        sqlite3_exec(db, sql_command, callback1, email_addr, &zErrMsg);

                                        send_mail_declined(user, email_addr, ans);
                                    }
                                    else
                                    {
                                        bzero(sql_command, 4000);
                                        sprintf(sql_command, "INSERT INTO SIGNEDUP VALUES ('%s', %s, 'necaz')", user, ans);
                                        sqlite3_exec(db, sql_command, NULL, NULL, &zErrMsg);

                                        bzero(sql_command, 4000);
                                        sprintf(sql_command, "UPDATE CHAMPIONSHIPS SET ENTERED = ENTERED + 1 WHERE ID = '%s';", ans);
                                        sqlite3_exec(db, sql_command, NULL, NULL, &zErrMsg);

                                        bzero(sql_command, 4000);
                                        sprintf(sql_command, "SELECT RULE, DECIDER, ENTERED, PLAYERNUM FROM CHAMPIONSHIPS WHERE ID='%s';", ans);
                                        bzero(result, 1000);
                                        sqlite3_exec(db, sql_command, callback1, result, &zErrMsg);

                                        char *rule, *decider, *players_entered, *total_players;

                                        rule = strtok(result, "\n");
                                        int regula = atoi(rule);
                                        decider = strtok(NULL, "\n");
                                        players_entered = strtok(NULL, "\n");
                                        total_players = strtok(NULL, "\n");

                                        int is_playernum_ok = 1;
                                        if (atoi(players_entered) == atoi(total_players))
                                            is_playernum_ok = 0;

                                        if (is_playernum_ok == 0)
                                        {
                                            char check_res[1000];
                                            bzero(sql_command, 4000);
                                            sprintf(sql_command, "SELECT USERNAME FROM SIGNEDUP WHERE CHAMPID='%s';", ans);
                                            bzero(check_res, 1000);
                                            sqlite3_exec(db, sql_command, callback1, check_res, &zErrMsg);

                                            while (strlen(check_res) > 1)
                                            {
                                                char emails[4000];
                                                if (strcmp(decider, "1") == 0)
                                                {
                                                    bzero(sql_command, 4000);
                                                    sprintf(sql_command, "SELECT USERNAME FROM SIGNEDUP WHERE CHAMPID='%s' ORDER BY USERNAME;", ans);
                                                    bzero(result, 1000);
                                                    sqlite3_exec(db, sql_command, callback1, result, &zErrMsg);
                                                }
                                                else if (strcmp(decider, "2") == 0)
                                                {
                                                    bzero(sql_command, 4000);
                                                    sprintf(sql_command, "SELECT USERNAME FROM SIGNEDUP WHERE CHAMPID='%s' ORDER BY RANDOM();", ans);
                                                    bzero(result, 1000);
                                                    sqlite3_exec(db, sql_command, callback1, result, &zErrMsg);
                                                }

                                                if (regula == 1)
                                                {
                                                    printf("am ajuns pe regula 1\n");
                                                    char *token = strtok(result, "\n");
                                                    while (token != NULL)
                                                    {
                                                        char *first_user = token;
                                                        char *second_user = strtok(NULL, "\n");

                                                        printf("%s - %s\n", first_user, second_user);

                                                        bzero(sql_command, 4000);
                                                        sprintf(sql_command, "SELECT EMAIL FROM USERINFO WHERE NAME='%s' OR NAME='%s';", first_user, second_user);
                                                        bzero(emails, 4000);
                                                        sqlite3_exec(db, sql_command, callback1, emails, &zErrMsg);

                                                        char *first_mail = strtok(emails, "\n");
                                                        char *second_mail = strtok(NULL, "\n");
                                                        char *date = calculate_date();

                                                        bzero(sql_command, 4000);
                                                        sprintf(sql_command, "UPDATE SIGNEDUP SET GAMEDATE='%s' WHERE CHAMPID='%s' AND (USERNAME='%s' OR USERNAME='%s');", date, first_user, second_user);
                                                        sqlite3_exec(db, sql_command, NULL, NULL, &zErrMsg);

                                                        printf("am ajuns aici inainte de mail\n");
                                                        printf("jucatorii sunt: %s si %s", first_user, second_user);
                                                        // send_mail_1v1(first_user, second_user, date, first_mail, second_mail, ans);
                                                        // send_mail_1v1(second_user, first_user, date, second_mail, first_mail, ans);

                                                        play_match_1v1(first_user, second_user, ans, date);

                                                        token = strtok(NULL, "\n");
                                                    }
                                                }
                                                else if (regula == 2)
                                                {
                                                    char *token = strtok(result, "\n");
                                                    while (token != NULL)
                                                    {
                                                        char *first_user = token;
                                                        char *second_user = strtok(NULL, "\n");
                                                        char *third_user = strtok(NULL, "\n");
                                                        char *fourth_user = strtok(NULL, "\n");

                                                        printf("%s, %s vs. %s, %s\n", first_user, second_user, third_user, fourth_user);

                                                        bzero(sql_command, 4000);
                                                        sprintf(sql_command, "SELECT EMAIL FROM USERINFO WHERE NAME='%s' OR NAME='%s' OR NAME='%s' OR NAME='%s';", first_user, second_user, third_user, fourth_user);
                                                        bzero(emails, 4000);
                                                        sqlite3_exec(db, sql_command, callback1, emails, &zErrMsg);

                                                        char *first_mail = strtok(emails, "\n");
                                                        char *second_mail = strtok(NULL, "\n");
                                                        char *third_mail = strtok(NULL, "\n");
                                                        char *fourth_mail = strtok(NULL, "\n");
                                                        char *date = calculate_date();

                                                        bzero(sql_command, 4000);
                                                        sprintf(sql_command, "UPDATE SIGNEDUP SET GAMEDATE='%s' WHERE CHAMPID='%s' AND (USERNAME='%s' OR USERNAME='%s' OR USERNAME='%s' OR USERNAME='%s');", date, first_user, second_user, third_user, fourth_user);
                                                        sqlite3_exec(db, sql_command, NULL, NULL, &zErrMsg);

                                                        // send_mail_2v2(first_user, second_user, third_user, fourth_user, first_mail, date, ans);
                                                        // send_mail_2v2(second_user, first_user, third_user, fourth_user, second_mail, date, ans);
                                                        // send_mail_2v2(third_user, fourth_user, first_user, second_user, third_mail, date, ans);
                                                        // send_mail_2v2(fourth_user, third_user, first_user, second_user, fourth_mail, date, ans);

                                                        play_match_2v2(first_user, second_user, third_user, fourth_user, ans, date);

                                                        token = strtok(NULL, "\n");
                                                    }
                                                }

                                                bzero(sql_command, 4000);
                                                sprintf(sql_command, "SELECT USERNAME FROM SIGNEDUP WHERE CHAMPID='%s';", ans);
                                                bzero(check_res, 1000);
                                                sqlite3_exec(db, sql_command, callback1, check_res, &zErrMsg);
                                            }

                                            bzero(sql_command, 4000);
                                            sprintf(sql_command, "DELETE FROM CHAMPIONSHIPS WHERE ID='%s';", ans);
                                            sqlite3_exec(db, sql_command, NULL, NULL, &zErrMsg);
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
                        sqlite3_exec(db, sql_command, callback1, result, &zErrMsg);

                        if (strlen(result) > 1)
                        {
                            write(client, "[Server] Please enter date you would like to reschedule to (format as dd.mm.yyyy): ", 83);
                            char date_candidate[100];

                            bzero(date_candidate, 100);
                            read(client, date_candidate, 100);

                            if (check_date(date_candidate) == 0)
                            {
                                write(client, "[Server] Date entered correctly.\n[Server] Please enter the time you would like to reschedule to (format as hh:mm)\n[Server] Please note that the hour must be between 10 and 18: ", 176);
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
                else if (strcmp(received, "change-username") == 0)
                {
                    if (!logged)
                    {
                        write(client, "[Server] Command unavailable. Log in to use this command.\n[Server] Enter command: ", 82);
                    }
                    else
                    {
                        write(client, "[Server] Are you sure you want to change your username? [y/n]: ", 63);
                        char ans[5];
                        bzero(ans, 5);

                        read(client, ans, 5);

                        if (strcmp(ans, "y") == 0 || strcmp(ans, "Y") == 0)
                        {
                            write(client, "[Server] Enter new username: ", 29);
                            char user_to_set[1000];
                            bzero(user_to_set, 1000);

                            read(client, user_to_set, 1000);

                            if (check_username(user_to_set))
                            {
                                char select_users[1000];
                                bzero(select_users, 1000);
                                bzero(sql_command, 4000);
                                sprintf(sql_command, "SELECT NAME FROM USERINFO WHERE NAME='%s';", user_to_set);
                                sqlite3_exec(db, sql_command, callback1, select_users, &zErrMsg);

                                if (strlen(select_users) > 1)
                                {
                                    write(client, "[Server] Username already exists. Operation will not continue.\n[Server] Enter command: ", 87);
                                }
                                else
                                {
                                    bzero(sql_command, 4000);
                                    sprintf(sql_command, "UPDATE USERINFO SET NAME='%s' WHERE NAME='%s';", user_to_set, user);
                                    sqlite3_exec(db, sql_command, NULL, NULL, &zErrMsg);

                                    bzero(sql_command, 4000);
                                    sprintf(sql_command, "UPDATE SIGNEDUP SET USERNAME='%s' WHERE USERNAME='%s';", user_to_set, user);
                                    sqlite3_exec(db, sql_command, NULL, NULL, &zErrMsg);

                                    bzero(user, 1000);
                                    strcpy(user, user_to_set);

                                    write(client, "[Server] Username changed successfully.\n[Server] Enter command: ", 64);
                                }
                            }
                            else
                            {
                                write(client, "[Server] Username cannot contain special characters! Operation will not continue.\n[Server] Enter command: ", 106);
                            }
                        }
                        else if (strcmp(ans, "n") == 0 || strcmp(ans, "N") == 0)
                        {
                            write(client, "[Server] Operation will not continue.\n[Server] Enter command: ", 62);
                        }
                        else
                        {
                            write(client, "[Server] Wrong input. Operation will not continue.\n[Server] Enter command: ", 75);
                        }
                    }
                }
                else if (strcmp(received, "change-email") == 0)
                {
                    if (!logged)
                    {
                        write(client, "[Server] Command unavailable. Log in to use this command.\n[Server] Enter command: ", 82);
                    }
                    else
                    {
                        write(client, "[Server] Are you sure you want to change your e-mail address? [y/n]: ", 69);

                        char ans[5];
                        bzero(ans, 5);
                        read(client, ans, 5);

                        if (strcmp(ans, "y") == 0 || strcmp(ans, "Y") == 0)
                        {
                            write(client, "[Server] Please enter your new e-mail address: ", 47);

                            char email_to_set[1000];
                            bzero(email_to_set, 1000);
                            read(client, email_to_set, 1000);

                            char result[1000];
                            bzero(result, 1000);
                            bzero(sql_command, 4000);
                            sprintf(sql_command, "SELECT EMAIL FROM USERINFO;", user);
                            sqlite3_exec(db, sql_command, callback1, result, &zErrMsg);

                            int flag_email = 0;

                            char *token = strtok(result, "\n");

                            while (token)
                            {
                                printf("%s\n", token);
                                if (strcmp(email_to_set, token) == 0)
                                {
                                    flag_email = 1;
                                    write(client, "[Server] E-mail already associated with an account. Operation will not continue\n[Server] Enter command: ", 104);
                                    break;
                                }

                                token = strtok(NULL, "\n");
                            }

                            if (flag_email == 0)
                            {
                                if (check_email(email_to_set))
                                {
                                    bzero(sql_command, 4000);
                                    sprintf(sql_command, "UPDATE USERINFO SET EMAIL='%s' WHERE NAME='%s';", email_to_set, user);
                                    sqlite3_exec(db, sql_command, NULL, NULL, &zErrMsg);

                                    write(client, "[Server] E-mail changed successfully.\n[Server] Enter command: ", 63);
                                }
                                else
                                {
                                    write(client, "[Server] Invalid e-mail. No changes have been made.\n[Server] Enter command: ", 76);
                                }
                            }
                        }
                        else if (strcmp(ans, "n") == 0 || strcmp(ans, "N") == 0)
                        {
                            write(client, "[Server] Operation will not continue.\n[Server] Enter command: ", 62);
                        }
                        else
                        {
                            write(client, "[Server] Wrong input. Operation will not continue.\n[Server] Enter command: ", 75);
                        }
                    }
                }
                else if (strcmp(received, "change-password") == 0)
                {
                    if (!logged)
                    {
                        write(client, "[Server] Command unavailable. Log in to use this command.\n[Server] Enter command: ", 82);
                    }
                    else
                    {
                        write(client, "[Server] Are you sure you want to change your password? [y/n]: ", 63);

                        char ans[5];
                        bzero(ans, 5);
                        read(client, ans, 5);

                        if (strcmp(ans, "y") == 0 || strcmp(ans, "Y") == 0)
                        {
                            write(client, "[Server] Enter your current password: ", 38);
                            char pass_ans[1000];
                            bzero(pass_ans, 1000);
                            read(client, pass_ans, 1000);

                            if (strcmp(pass_ans, pass) == 0)
                            {
                                char pass_confirm[1000];
                                bzero(pass_confirm, 1000);
                                bzero(pass_ans, 1000);

                                write(client, "[Server] Enter new password: ", 29);
                                read(client, pass_ans, 1000);
                                write(client, "[Server] Confirm new password: ", 31);
                                read(client, pass_confirm, 1000);

                                if (strcmp(pass_confirm, pass_ans) == 0)
                                {
                                    bzero(sql_command, 4000);
                                    sprintf(sql_command, "UPDATE USERINFO SET PASS='%s' WHERE NAME='%s';", pass_confirm, user);
                                    sqlite3_exec(db, sql_command, NULL, NULL, &zErrMsg);

                                    write(client, "[Server] Password changed successfully.\n[Server] Enter command: ", 64);
                                }
                                else
                                {
                                    write(client, "[Server] Passwords do not match. Operation will not continue.\n[Server] Enter command: ", 86);
                                }
                            }
                            else
                            {
                                write(client, "[Server] Current password incorrect. Operation will not continue.\n[Server] Enter command: ", 90);
                            }
                        }
                        else if (strcmp(ans, "n") == 0 || strcmp(ans, "N") == 0)
                        {
                            write(client, "[Server] Operation will not continue.\n[Server] Enter command: ", 62);
                        }
                        else
                        {
                            write(client, "[Server] Wrong input. Operation will not continue.\n[Server] Enter command: ", 75);
                        }
                    }
                }
                else
                // end of control sequence
                // if reached, the command is considered unknown and user is informed
                {
                    write(client, "[Server] Unknown command entered. Please check spelling and try again.\n[Server] Enter command: ", 95);
                }
            }
        }
    }
}
