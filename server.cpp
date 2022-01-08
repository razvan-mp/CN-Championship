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

#define RED "\e[1;31m"
#define GREEN "\e[1;32m"
#define YELLOW "\e[1;33m"
#define BLUE "\e[1;34m"
#define MAGENTA "\e[1;35m"
#define CYAN "\e[1;36m"
#define RESET "\x1b[0m"

#define PORT 2728

std::vector<std::string> players;

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

static int callback2(void *ans, int argc, char **argv, char **azColName)
{
    std::string tmp(argv[0]);
    players.push_back(tmp);

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
        if (!power_of_two(atoi(str)))
            return 1;
    }
    else if (decider == 2)
    {
        if (!power_of_four(atoi(str)))
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

    srand(time(nullptr));

    int option = rand() % 4;
    struct tm date = {0, possibilities[option], rand() % (18 - 10 + 1) + 10};

    time_t t = time(nullptr);
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

    char *mm = strtok(nullptr, ".");
    if (strlen(mm) != 2 || check_id_number(mm) == 1)
    {
        return 1;
    }

    char *yyyy = strtok(nullptr, ".");
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
            char *zErrMsg = nullptr;
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

            if (write(client, CYAN "[Server] " BLUE "Enter " YELLOW "help" BLUE " for a list of commands or enter command: " RESET, 93) <= 0)
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
                        write(client, CYAN "[Server] " BLUE "Available commands:\n\t" BLUE "1. " YELLOW "help\n\t" BLUE "2. " YELLOW "login\n\t" BLUE "3. " YELLOW "register\n\t" BLUE "4. " YELLOW "exit\n" CYAN "[Server] " BLUE "Enter command: " RESET, 182);
                    }

                    if (logged && userType == 0)
                    {
                        write(client, CYAN "[Server] " BLUE "Available commands:\n\t" BLUE "1. " YELLOW "help\n\t" BLUE "2. " YELLOW "create-championship\n\t" BLUE "3. " YELLOW "view-championships\n\t" BLUE "4. " YELLOW "enter-championship\n\t" BLUE "5. " YELLOW "change-date\n\t" BLUE "6. " YELLOW "add-admin\n\t" BLUE "7. " YELLOW "logout\n\t" BLUE "8. " YELLOW "exit\n" CYAN "[Server] " BLUE "Enter command: " RESET, 326);
                    }

                    if (logged && userType == 1)
                    {
                        write(client, CYAN "[Server] " BLUE "Available commands:\n\t" BLUE "1. " YELLOW "help\n\t" BLUE "2. " YELLOW "view-championships\n\t" BLUE "3. " YELLOW "enter-championship\n\t" BLUE "4. " YELLOW "change-date\n\t" BLUE "5. " YELLOW "logout\n\t" BLUE "6. " YELLOW "exit\n" CYAN "[Server] " BLUE "Enter command: " RESET, 260);
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
                        write(client, CYAN "[Server]" BLUE " Please provide a username: " RESET, 54);
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
                            write(client, RED "[Server] " RED "User already exists! Try logging in.\n" CYAN "[Server] " BLUE "Enter command: " RESET, 102);
                        }
                        else if (!check_username(user))
                        {
                            write(client, RED "[Server] " RED "Username cannot contain special characters! Operation will not continue.\n" CYAN "[Server] " BLUE "Enter command: " RESET, 138);
                        }
                        else
                        {
                            // check if user is in whitelist
                            FILE *fp;
                            char *line = nullptr;
                            size_t len = 0;
                            ssize_t bytes_read;

                            fp = fopen("whitelist.txt", "r");
                            if (fp == nullptr)
                                exit(EXIT_FAILURE);

                            uType = 1;
                            while ((bytes_read = getline(&line, &len, fp)) != -1)
                            {
                                if (strncmp(user, line, strlen(line) - 1) == 0)
                                    uType = 0;
                            }

                            fclose(fp);

                            write(client, CYAN "[Server] " BLUE "Please provide a password: " RESET, 54);
                            read(client, pass, 1000);
                            write(client, CYAN "[Server] " BLUE "Please provide your email address: " RESET, 62);
                            read(client, mail, 1000);

                            // create sql operation string
                            bzero(sql_command, 4000);
                            sprintf(sql_command, "INSERT INTO USERINFO VALUES ('%s', '%s', '%s', %d, 0);", user, pass, mail, uType);

                            write(client, CYAN "[Server] " BLUE "Account created successfully.\n" CYAN "[Server] " BLUE "Enter " YELLOW "help" BLUE " for a list of commands or enter command: " RESET, 146);
                            rc = sqlite3_exec(db, sql_command, nullptr, nullptr, &zErrMsg);

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
                        write(client, RED "[Server] Log out to use this command.\n" CYAN "[Server] " BLUE "Enter command: " RESET, 87);
                    }
                }
                else if (strcmp(received, "login") == 0)
                {
                    if (!logged)
                    {
                        char result[4000];
                        printf("[Server] User requested login. Sending user and pass prompt...\n");
                        fflush(stdout);

                        write(client, CYAN "[Server] " BLUE "Enter your username: " RESET, 48);
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
                            write(client, CYAN "[Server]" BLUE " Enter your password: " RESET, 48);

                            bzero(pass, 1000);
                            read(client, pass, 1000);

                            bzero(sql_command, 4000);
                            sprintf(sql_command, "SELECT NAME, PASS, TYPE FROM USERINFO WHERE NAME='%s' AND PASS='%s'", user, pass);
                            bzero(result, 4000);
                            rc = sqlite3_exec(db, sql_command, callback1, result, &zErrMsg);

                            if (strlen(result) > 1)
                            {
                                char *typee = strtok(result, "\n");
                                typee = strtok(nullptr, "\n");
                                typee = strtok(nullptr, "\n");
                                logged = 1;
                                userType = atoi(typee);
                                printf("[Server] User/pass combo correct.\n");
                                fflush(stdout);

                                write(client, CYAN "[Server] " GREEN "Logged in successfully.\n" CYAN "[Server] " BLUE "Enter " YELLOW "help" BLUE " for a list of commands or enter command: " RESET, 140);
                            }
                            else
                            {
                                printf("[Server] Wrong login info.\n");
                                fflush(stdout);
                                write(client, RED "[Server] Wrong user/pass combination.\n" CYAN "[Server] " BLUE "Enter command: " RESET, 87);
                            }
                        }
                        else
                        {
                            printf("[Server] Username does not exist.\n");
                            fflush(stdout);
                            write(client, RED "[Server] Username does not exist. Operation will not continue.\n" CYAN "[Server] " BLUE "Enter command: " RESET, 112);
                        }
                    }
                    else
                    {
                        printf("[Server] User entered 'login' while logged in.\n");
                        fflush(stdout);
                        write(client, RED "[Server] Log out to use this command.\n" CYAN "[Server] " BLUE "Enter command: " RESET, 87);
                    }
                }
                else if (strcmp(received, "logout") == 0)
                {
                    if (!logged)
                    {
                        printf("[Server] User tried logging out while not being logged in.\n");
                        fflush(stdout);
                        write(client, RED "[Server] You are not logged in. Command unavailable.\n" CYAN "[Server] " BLUE "Enter command: " RESET, 102);
                    }
                    else
                    {
                        logged = 0;
                        userType = -1;

                        printf("[Server] User logged out.\n");
                        fflush(stdout);
                        write(client, CYAN "[Server] " GREEN "Logged out successfully.\n" CYAN "[Server] " BLUE "Enter command: " RESET, 90);
                    }
                }
                else if (strcmp(received, "add-admin") == 0)
                {
                    if (!logged)
                    {
                        printf("[Server] User tried adding admin while not being logged in.\n");
                        fflush(stdout);
                        write(client, RED "[Server] You are not logged in. Command unavailable.\n" CYAN "[Server] " BLUE "Enter command: " RESET, 102);
                    }
                    else
                    {
                        write(client, CYAN "[Server] " BLUE "Provide username to add to whitelist: " RESET, 65);

                        char user_to_add[1000];
                        bzero(user_to_add, 1000);
                        read(client, user_to_add, 1000);

                        // check if user is in whitelist
                        int flag = 0; // user is not in whitelist
                        FILE *fp;
                        char *line = nullptr;
                        size_t len = 0;
                        ssize_t bytes_read;

                        fp = fopen("whitelist.txt", "r");
                        if (fp == nullptr)
                            exit(EXIT_FAILURE);

                        while ((bytes_read = getline(&line, &len, fp)) != -1)
                        {
                            if (strncmp(user_to_add, line, strlen(line) - 1) == 0)
                                flag = 1;
                        }

                        fclose(fp);

                        if (flag == 1)
                        {
                            write(client, RED "[Server] Username already whitelisted.\n" CYAN "[Server] " BLUE "Enter command: " RESET, 88);
                        }
                        else
                        {
                            fp = fopen("whitelist.txt", "a");

                            fprintf(fp, "%s", user_to_add);
                            fprintf(fp, "\n");

                            fclose(fp);

                            write(client, CYAN "[Server] " GREEN "Added username to whitelist successfully.\n" CYAN "[Server] " BLUE "Enter command: " RESET, 107);
                        }
                    }
                }
                else if (strcmp(received, "create-championship") == 0)
                {
                    if (!logged)
                    {
                        printf("[Server] User tried creating championship while not being logged in.\n");
                        fflush(stdout);
                        write(client, RED "[Server] You are not logged in. Command unavailable.\n" CYAN "[Server] " BLUE "Enter command: " RESET, 102);
                    }
                    else
                    {
                        if (userType == 0)
                        {
                            char game_name[1000], playernum[1000], rules[1000], decider[1000];
                            printf("[Server] User intends to create championship. Sending prompts to user...\n");
                            fflush(stdout);

                            // specify game
                            write(client, CYAN "[Server] " BLUE "Please enter game name for championship: " RESET, 68);
                            bzero(game_name, 1000);
                            read(client, game_name, 1000);

                            // set rules
                            write(client, CYAN "[Server] " BLUE "Please choose an option for rules: \n\t1. " YELLOW "1 V 1\n\t" BLUE "2. " YELLOW "2 V 2\n" CYAN "[Server] " BLUE "Option: " RESET, 135);
                            bzero(rules, 1000);
                            read(client, rules, 1000);

                            int flag_rules = -1;

                            if (strcmp(rules, "1") == 0)
                            {
                                flag_rules = 1;
                                write(client, CYAN "[Server]" GREEN " 1 V 1 rule set.\n" RESET, 43);
                            }
                            else if (strcmp(rules, "2") == 0)
                            {
                                flag_rules = 1;
                                write(client, CYAN "[Server] " GREEN "2 V 2 rule set.\n" RESET, 43);
                            }
                            else
                            {
                                flag_rules = 1;
                            }

                            // set decider
                            write(client, CYAN "[Server] " BLUE "Please choose an option for deciding matchups: \n\t1. " YELLOW "Random\n\t" BLUE "2. " YELLOW "Alphabetical\n" CYAN "[Server] " BLUE "Option: " RESET, 155);
                            bzero(decider, 1000);
                            read(client, decider, 1000);

                            int flag_decider = -1;

                            if (strcmp(decider, "1") == 0)
                            {
                                flag_decider = 0;
                                write(client, CYAN "[Server] " BLUE "Random decider rule set.\n" RESET, 53);
                            }
                            else if (strcmp(decider, "2") == 0)
                            {
                                flag_decider = 0;
                                write(client, CYAN "[Server] " BLUE "Alphabetical decider rule set.\n" RESET, 58);
                            }
                            else
                            {
                                flag_decider = 1;
                            }

                            // set player number
                            write(client, CYAN "[Server] " BLUE "Please enter player number: " RESET, 55);
                            bzero(playernum, 1000);
                            read(client, playernum, 1000);

                            write(client, CYAN "[Server] " GREEN "Player number set. " BLUE "Trying to create championship...\n" RESET, 86);

                            if ((atoi(decider) == 1 || atoi(decider) == 2) && (atoi(rules) == 1 || atoi(rules) == 2) && checkplayernum(playernum, atoi(decider) == 0))
                            {
                                bzero(sql_command, 1000);
                                sprintf(sql_command, "INSERT INTO CHAMPIONSHIPS VALUES ((SELECT MAX(ID) FROM CHAMPIONSHIPS) + 1, '%s', %d, %d, %d, 0);", game_name, atoi(playernum), atoi(rules), atoi(decider));
                                rc = sqlite3_exec(db, sql_command, nullptr, nullptr, &zErrMsg);
                            }
                            else
                            {
                                write(client, RED "[Server] Wrong inputs. Championship not created.\n" CYAN "[Server] " BLUE "Enter command: " RESET, 98);
                            }
                        }
                        else
                        {
                            printf("[Server] Regular user tried using admin command.\n");
                            fflush(stdout);
                            write(client, RED "[Server] You do not have the permission to use this command.\n" CYAN "[Server] " BLUE "Enter command: " RESET, 110);
                        }
                    }
                }
                else if (strcmp(received, "view-championships") == 0)
                {
                    if (!logged)
                    {
                        printf("[Server] User tried viewing championships while not being logged in.\n");
                        fflush(stdout);
                        write(client, RED "[Server] You are not logged in. Command unavailable.\n" CYAN "[Server] " BLUE "Enter command: " RESET, 102);
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
                                game_name = strtok(nullptr, "\n");
                                playercount = strtok(nullptr, "\n");
                                rules = strtok(nullptr, "\n");
                                decider = strtok(nullptr, "\n");
                                playerentered = strtok(nullptr, "\n");

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

                                sprintf(championships + strlen(championships), BLUE "%s\t" YELLOW "%s\t%s\t\t%s\t\t%s\t\t%s\n" RESET, champ_id, game_name, playercount, rule, decide, playerentered);

                                id++;
                            }
                            sprintf(championships + strlen(championships), CYAN "[Server] " BLUE "Enter command:");
                            write(client, championships, sizeof(championships));
                        }
                        else
                        {
                            write(client, MAGENTA "[Server] No championships to show.\n" CYAN "[Server] " BLUE "Enter command: " RESET, 84);
                        }
                    }
                }
                else if (strcmp(received, "enter-championship") == 0)
                {
                    if (!logged)
                    {
                        printf("[Server] User tried viewing championships while not being logged in.\n");
                        fflush(stdout);
                        write(client, RED "[Server] You are not logged in. Command unavailable.\n" CYAN "[Server] " BLUE "Enter command: " RESET, 102);
                    }
                    else
                    {
                        printf("[Server] User wants to enter championship. Sending prompts...\n");
                        fflush(stdout);
                        write(client, CYAN "[Server] " BLUE "Please enter ID for championship you want to enter: " RESET, 79);

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
                            write(client, RED "[Server] ID is not a number. Operation will not continue.\n" CYAN "[Server] " BLUE "Enter command: " RESET, 107);
                        }
                        else
                        {
                            if (atoi(ans) <= 0 || atoi(ans) > atoi(max_id))
                            {
                                write(client, RED "[Server] No championship with this ID. Operation will not continue.\n" CYAN "[Server] " BLUE "Enter command: " RESET, 117);
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
                                    write(client, RED "[Server] Already entered this championship. Operation will not execute.\n" CYAN "[Server] " BLUE "Enter command: " RESET, 121);
                                }
                                else
                                {
                                    bzero(sql_command, 4000);
                                    sprintf(sql_command, "SELECT PLAYERNUM, ENTERED FROM CHAMPIONSHIPS WHERE ID=%s;", ans);
                                    bzero(result, 1000);
                                    sqlite3_exec(db, sql_command, callback1, result, &zErrMsg);

                                    char *playernum, *entered;

                                    playernum = strtok(result, "\n");
                                    entered = strtok(nullptr, "\n");

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
                                        sqlite3_exec(db, sql_command, nullptr, nullptr, &zErrMsg);

                                        bzero(sql_command, 4000);
                                        sprintf(sql_command, "UPDATE CHAMPIONSHIPS SET ENTERED = ENTERED + 1 WHERE ID = '%s';", ans);
                                        sqlite3_exec(db, sql_command, nullptr, nullptr, &zErrMsg);
                                    }

                                    bzero(sql_command, 4000);
                                    sprintf(sql_command, "SELECT RULE, DECIDER, ENTERED, PLAYERNUM FROM CHAMPIONSHIPS WHERE ID='%s';", ans);
                                    bzero(result, 1000);
                                    sqlite3_exec(db, sql_command, callback1, result, &zErrMsg);

                                    char *rule, *decider, *players_entered, *total_players;

                                    rule = strtok(result, "\n");
                                    int regula = atoi(rule);
                                    decider = strtok(nullptr, "\n");
                                    players_entered = strtok(nullptr, "\n");
                                    total_players = strtok(nullptr, "\n");

                                    int is_playernum_ok = 1;
                                    if (atoi(players_entered) == atoi(total_players))
                                        is_playernum_ok = 0;

                                    if (is_playernum_ok == 0)
                                    {
                                        bzero(sql_command, 4000);

                                        if (regula == 1)
                                        {
                                            if (strcmp(decider, "1") == 0)
                                            {
                                                char answer[4000];
                                                bzero(answer, 4000);
                                                bzero(sql_command, 4000);
                                                sprintf(sql_command, "SELECT USERNAME FROM SIGNEDUP WHERE CHAMPID='%s';", ans);
                                                sqlite3_exec(db, sql_command, callback1, answer, &zErrMsg);

                                                while (strlen(answer) > 1)
                                                {
                                                    char *tkn_1 = strtok(answer, "\n");
                                                    char* tkn = strtok(nullptr, "\n");
                                                    if (tkn == nullptr) // there's only one player left, which is the championship winner
                                                    {
                                                        bzero(sql_command, 4000);
                                                        sprintf(sql_command, "DELETE FROM CHAMPIONSHIPS WHERE ID='%s';", ans);
                                                        sqlite3_exec(db, sql_command, nullptr, nullptr, &zErrMsg);

                                                        bzero(sql_command, 4000);
                                                        sprintf(sql_command, "UPDATE USERINFO SET SCORE=SCORE+200 WHERE NAME='%s';", tkn_1);
                                                        sqlite3_exec(db, sql_command, nullptr, nullptr, &zErrMsg);

                                                        bzero(sql_command, 40000);
                                                        sprintf(sql_command, "DELETE FROM SIGNEDUP WHERE USENAME='%s' AND CHAMPID='%s';", tkn_1, ans);
                                                        sqlite3_exec(db, sql_command, nullptr, nullptr, &zErrMsg);
                                                        
                                                        matches = fopen("matches.txt", "a");
                                                        fprintf(matches, "Player %s won championship with ID %s and got 200 points.\n", tkn_1, ans);
                                                        fclose(matches);

                                                        break;
                                                    }
                                                    else
                                                    {

                                                    }

                                                    sprintf(sql_command, "SELECT USERNAME FROM SIGNEDUP WHERE CHAMPID='%s' ORDER BY USERNAME;", ans);
                                                    sqlite3_exec(db, sql_command, callback2, nullptr, &zErrMsg);

                                                    int i = 0;
                                                    while (i < players.size())
                                                    {
                                                        char first_player[players[i].length()];
                                                        strcpy(first_player, players[i].c_str());

                                                        i++;

                                                        char second_player[players[i].length()];
                                                        strcpy(second_player, players[i].c_str());

                                                        i++;

                                                        char result[1000];
                                                        bzero(result, 1000);
                                                        bzero(sql_command, 4000);
                                                        sqlite3_exec(db, sql_command, callback1, result, &zErrMsg);

                                                        char *first_mail = strtok(result, "\n");
                                                        char *second_mail = strtok(nullptr, "\n");

                                                        char *date = calculate_date();
                                                        // send_mail_1v1(first_player, second_player, date, first_mail, second_mail, ans);
                                                        // send_mail_1v1(second_player, first_player, date, second_mail, first_mail, ans);

                                                        play_match_1v1(first_player, second_player, ans, date);
                                                        bzero(answer, 4000);
                                                        bzero(sql_command, 4000);
                                                        sprintf(sql_command, "SELECT USERNAME FROM SIGNEDUP WHERE CHAMPID='%s';", ans);
                                                        sqlite3_exec(db, sql_command, callback1, answer, &zErrMsg);
                                                    }
                                                }
                                            }
                                        }
                                    }

                                    write(client, CYAN "[Server] " GREEN "Request for entering championship sent. You will receive an e-mail at " YELLOW "%s" BLUE " with further information.\n" CYAN "[Server] " BLUE "Enter command: " RESET, 178);
                                }
                            }
                        }
                    }
                }
                else if (strcmp(received, "change-date") == 0)
                {
                    write(client, CYAN "[Server] " BLUE "Enter ID for championship you want to reschedule: " RESET, 77);
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
                            write(client, CYAN "[Server] " BLUE "Please enter date you would like to reschedule to (format as " YELLOW "dd.mm.yyyy" BLUE "): " RESET, 115);
                            char date_candidate[100];

                            bzero(date_candidate, 100);
                            read(client, date_candidate, 100);

                            if (check_date(date_candidate) == 0)
                            {
                                write(client, CYAN "[Server] " GREEN "Date entered correctly.\n" CYAN "[Server] " BLUE "Please enter the time you would like to reschedule to (format as " YELLOW "hh:mm" BLUE ")\n" CYAN "[Server] " BLUE "Please note that the hour must be between 10 and 18: " RESET, 236);
                            }
                            else
                            {
                                write(client, RED "[Server] Incorrect date format. Operation will not continue.\n" CYAN "[Server] " BLUE "Enter command: " RESET, 110);
                            }
                        }
                        else
                        {
                            write(client, RED "[Server] You have not entered this championship. Operation will not continue.\n" CYAN "[Server] " BLUE "Enter command: " RESET, 127);
                        }
                    }
                    else
                    {
                        write(client, RED "[Server] Invalid ID. Operation will not continue.\n" CYAN "[Server] " BLUE "Enter command: " RESET, 99);
                    }
                }
                else if (strcmp(received, "change-username") == 0)
                {
                    if (!logged)
                    {
                        write(client, RED "[Server] You are not logged in. Command unavailable.\n" CYAN "[Server] " BLUE "Enter command: " RESET, 102);
                    }
                    else
                    {
                        write(client, CYAN "[Server] " BLUE "Are you sure you want to change your username? " YELLOW "[y/n]" BLUE ": " RESET, 95);
                        char ans[5];
                        bzero(ans, 5);

                        read(client, ans, 5);

                        if (strcmp(ans, "y") == 0 || strcmp(ans, "Y") == 0)
                        {
                            write(client, CYAN "[Server] " BLUE "Enter new username: " RESET, 47);
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
                                    write(client, RED "[Server] Username already exists. Operation will not continue.\n" CYAN "[Server] " BLUE "Enter command: " RESET, 112);
                                }
                                else
                                {
                                    bzero(sql_command, 4000);
                                    sprintf(sql_command, "UPDATE USERINFO SET NAME='%s' WHERE NAME='%s';", user_to_set, user);
                                    sqlite3_exec(db, sql_command, nullptr, nullptr, &zErrMsg);

                                    bzero(sql_command, 4000);
                                    sprintf(sql_command, "UPDATE SIGNEDUP SET USERNAME='%s' WHERE USERNAME='%s';", user_to_set, user);
                                    sqlite3_exec(db, sql_command, nullptr, nullptr, &zErrMsg);

                                    bzero(user, 1000);
                                    strcpy(user, user_to_set);

                                    write(client, CYAN "[Server] " GREEN "Username changed successfully.\n" CYAN "[Server] " BLUE "Enter command: " RESET, 96);
                                }
                            }
                            else
                            {
                                write(client, RED "[Server] Username cannot contain special characters! Operation will not continue.\n" CYAN "[Server] " BLUE "Enter command: " RESET, 131);
                            }
                        }
                        else if (strcmp(ans, "n") == 0 || strcmp(ans, "N") == 0)
                        {
                            write(client, RED "[Server] Operation will not continue.\n" CYAN "[Server] " BLUE "Enter command: " RESET, 87);
                        }
                        else
                        {
                            write(client, RED "[Server] Wrong input. Operation will not continue.\n" CYAN "[Server] " BLUE "Enter command: " RESET, 100);
                        }
                    }
                }
                else if (strcmp(received, "change-email") == 0)
                {
                    if (!logged)
                    {
                        write(client, RED "[Server] You are not logged in. Command unavailable.\n" CYAN "[Server] " BLUE "Enter command: " RESET, 102);
                    }
                    else
                    {
                        write(client, CYAN "[Server] " BLUE "Are you sure you want to change your e-mail address? " YELLOW "[y/n]" BLUE ": " RESET, 101);

                        char ans[5];
                        bzero(ans, 5);
                        read(client, ans, 5);

                        if (strcmp(ans, "y") == 0 || strcmp(ans, "Y") == 0)
                        {
                            write(client, CYAN "[Server] " BLUE "Please enter your new e-mail address: " RESET, 65);

                            char email_to_set[1000];
                            bzero(email_to_set, 1000);
                            read(client, email_to_set, 1000);

                            char result[1000];
                            bzero(result, 1000);
                            bzero(sql_command, 4000);
                            sprintf(sql_command, "SELECT EMAIL FROM USERINFO;");
                            sqlite3_exec(db, sql_command, callback1, result, &zErrMsg);

                            int flag_email = 0;

                            char *token = strtok(result, "\n");

                            while (token)
                            {
                                printf("%s\n", token);
                                if (strcmp(email_to_set, token) == 0)
                                {
                                    flag_email = 1;
                                    write(client, RED "[Server] E-mail already associated with an account. Operation will not continue\n" CYAN "[Server] " BLUE "Enter command: " RESET, 129);
                                    break;
                                }

                                token = strtok(nullptr, "\n");
                            }

                            if (flag_email == 0)
                            {
                                if (check_email(email_to_set))
                                {
                                    bzero(sql_command, 4000);
                                    sprintf(sql_command, "UPDATE USERINFO SET EMAIL='%s' WHERE NAME='%s';", email_to_set, user);
                                    sqlite3_exec(db, sql_command, nullptr, nullptr, &zErrMsg);

                                    write(client, CYAN "[Server] " GREEN "E-mail changed successfully.\n" CYAN "[Server] " BLUE "Enter command: " RESET, 94);
                                }
                                else
                                {
                                    write(client, RED "[Server] Invalid e-mail. No changes have been made.\n" CYAN "[Server] " BLUE "Enter command: " RESET, 101);
                                }
                            }
                        }
                        else if (strcmp(ans, "n") == 0 || strcmp(ans, "N") == 0)
                        {
                            write(client, RED "[Server] Operation will not continue.\n" CYAN "[Server] " BLUE "Enter command: " RESET, 87);
                        }
                        else
                        {
                            write(client, RED "[Server] Wrong input. Operation will not continue.\n" CYAN "[Server] " BLUE "Enter command: " RESET, 100);
                        }
                    }
                }
                else if (strcmp(received, "change-password") == 0)
                {
                    if (!logged)
                    {
                        write(client, RED "[Server] You are not logged in. Command unavailable.\n" CYAN "[Server] " BLUE "Enter command: " RESET, 102);
                    }
                    else
                    {
                        write(client, CYAN "[Server] " BLUE "Are you sure you want to change your password? " YELLOW "[y/n]" BLUE ": " RESET, 95);

                        char ans[5];
                        bzero(ans, 5);
                        read(client, ans, 5);

                        if (strcmp(ans, "y") == 0 || strcmp(ans, "Y") == 0)
                        {
                            write(client, CYAN "[Server] " BLUE "Enter your current password: " RESET, 56);
                            char pass_ans[1000];
                            bzero(pass_ans, 1000);
                            read(client, pass_ans, 1000);

                            if (strcmp(pass_ans, pass) == 0)
                            {
                                char pass_confirm[1000];
                                bzero(pass_confirm, 1000);
                                bzero(pass_ans, 1000);

                                write(client, CYAN "[Server] " BLUE "Enter new password: " RESET, 47);
                                read(client, pass_ans, 1000);
                                write(client, CYAN "[Server] " BLUE "Confirm new password: " RESET, 49);
                                read(client, pass_confirm, 1000);

                                if (strcmp(pass_confirm, pass_ans) == 0)
                                {
                                    bzero(sql_command, 4000);
                                    sprintf(sql_command, "UPDATE USERINFO SET PASS='%s' WHERE NAME='%s';", pass_confirm, user);
                                    sqlite3_exec(db, sql_command, nullptr, nullptr, &zErrMsg);

                                    write(client, CYAN "[Server] " GREEN "Password changed successfully.\n" CYAN "[Server] " BLUE "Enter command: " RESET, 96);
                                }
                                else
                                {
                                    write(client, RED "[Server] Passwords do not match. Operation will not continue.\n" CYAN "[Server] " BLUE "Enter command: " RESET, 111);
                                }
                            }
                            else
                            {
                                write(client, RED "[Server] Current password incorrect. Operation will not continue.\n" CYAN "[Server] " BLUE "Enter command: " RESET, 115);
                            }
                        }
                        else if (strcmp(ans, "n") == 0 || strcmp(ans, "N") == 0)
                        {
                            write(client, RED "[Server] Operation will not continue.\n" CYAN "[Server] " BLUE "Enter command: " RESET, 87);
                        }
                        else
                        {
                            write(client, RED "[Server] Wrong input. Operation will not continue.\n" CYAN "[Server] " BLUE "Enter command: " RESET, 100);
                        }
                    }
                }
                else
                // end of control sequence
                // if reached, the command is considered unknown and user is informed
                {
                    write(client, RED "[Server] Unknown command entered. Please check spelling and try again.\n" CYAN "[Server] " BLUE "Enter command: " RESET, 120);
                }
            }
        }
    }
}
