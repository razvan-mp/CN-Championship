#include <iostream>
#include <vector>
#include <cstring>
#include <sqlite3.h>

FILE *matches;

// check if date for playing a match has passed
int has_date_passed(const char *date)
{
    return 0;
}

// "play" a 1v1 match
int play_match_1v1(const char *first, const char *second, const char *id, const char *date)
{
    sqlite3 *db;
    char *zErrMsg = 0;
    int rc;
    rc = sqlite3_open("userinfo.db", &db);

    char sql_command[4000];
    srand(time(NULL));
    int winner = rand() % 2;

    if (winner == 0)
    {
        // insert in file "first won against second in championship with ID : \n first got score_win points and second got score_lose points"
        bzero(sql_command, 4000);
        sprintf(sql_command, "DELETE FROM SIGNEDUP WHERE USERNAME='%s' AND CHAMPID='%s';", second, id);
        sqlite3_exec(db, sql_command, NULL, NULL, &zErrMsg);

        bzero(sql_command, 4000);
        sprintf(sql_command, "UPDATE USERINFO SET SCORE=CASE WHEN NAME='%s' THEN SCORE + 200 WHEN NAME='%s' THEN SCORE - 150 ELSE SCORE END", first, second);
        sqlite3_exec(db, sql_command, NULL, NULL, &zErrMsg);

        matches = fopen("matches.txt", "a");
        char to_append[1000];
        sprintf(to_append, "Championship ID: %s\n\tPlayer %s won against player %s on %s. Scores have been adjusted accordingly, with +200 for %s and -150 for %s.\n", id, first, second, date, first, second);
        fprintf(matches, "%s", to_append);
    }
    else
    {
        // the other way around
        bzero(sql_command, 4000);
        sprintf(sql_command, "DELETE FROM SIGNEDUP WHERE USERNAME='%s' AND CHAMPID='%s';", first, id);
        sqlite3_exec(db, sql_command, NULL, NULL, &zErrMsg);

        bzero(sql_command, 4000);
        sprintf(sql_command, "UPDATE USERINFO SET SCORE=CASE WHEN NAME='%s' THEN SCORE + 200 WHEN NAME='%s' THEN SCORE - 150 ELSE SCORE END", second, first);
        sqlite3_exec(db, sql_command, NULL, NULL, &zErrMsg);

        matches = fopen("matches.txt", "a");
        char to_append[1000];
        sprintf(to_append, "Championship ID: %s\n\tPlayer %s won against player %s on %s. Scores have been adjusted accordingly, with +200 for %s and -150 for %s.\n", id, second, first, date, second, first);
        fprintf(matches, "%s", to_append);
    }

    return winner;
}

// "play" a 2v2 match
int play_match_2v2(const char *first, const char *second, const char *third, const char *fourth, const char *id, const char* date)
{
    sqlite3 *db;
    char *zErrMsg = 0;
    int rc;
    rc = sqlite3_open("userinfo.db", &db);

    char sql_command[4000];
    int winner = rand() % 2;

    if (winner == 0)
    {
        // insert in file "first won against second in championship with ID : \n first got score_win points and second got score_lose points"
        bzero(sql_command, 4000);
        sprintf(sql_command, "DELETE FROM SIGNEDUP WHERE (USERNAME='%s' OR USERNAME='%s') AND CHAMPID='%s';", third, fourth, id);
        sqlite3_exec(db, sql_command, NULL, NULL, &zErrMsg);

        bzero(sql_command, 4000);
        sprintf(sql_command, "UPDATE USERINFO SET SCORE=CASE WHEN (NAME='%s' OR NAME='%s') THEN SCORE + 250 WHEN (NAME='%s' OR NAME='%s') THEN SCORE - 175 ELSE SCORE END", first, second, third, fourth);
        sqlite3_exec(db, sql_command, NULL, NULL, &zErrMsg);

        matches = fopen("matches.txt", "a");
        char to_append[1000];
        sprintf(to_append, "Championship ID: %s\n\tPlayers %s and %s won against players %s and %s on %s. Scores have been adjusted accordingly, with +250 for %s and %s and -175 for %s and %s.\n", id, first, second, third, fourth, date, first, second, third, fourth);
        fprintf(matches, "%s", to_append);
    }
    else
    {
        // the other way around
        bzero(sql_command, 4000);
        sprintf(sql_command, "DELETE FROM SIGNEDUP WHERE (USERNAME='%s' OR USERNAME='%s') AND CHAMPID='%s';", first, second, id);
        sqlite3_exec(db, sql_command, NULL, NULL, &zErrMsg);

        bzero(sql_command, 4000);
        sprintf(sql_command, "UPDATE USERINFO SET SCORE=CASE WHEN (NAME='%s' OR NAME='%s') THEN SCORE + 250 WHEN (NAME='%s' OR NAME='%s') THEN SCORE - 175 ELSE SCORE END", third, fourth, first, second);
        sqlite3_exec(db, sql_command, NULL, NULL, &zErrMsg);

        matches = fopen("matches.txt", "a");
        char to_append[1000];
        sprintf(to_append, "Championship ID: %s\n\tPlayers %s and %s won against players %s and %s on %s. Scores have been adjusted accordingly, with +250 for %s and %s and -175 for %s and %s.\n", id, third, fourth, first, second, date, third, fourth, first, second);
        fprintf(matches, "%s", to_append);
    }

    return winner;
}
