#include <iostream>
#include <vector>
#include <cstring>
#include <sqlite3.h>

int play_match_1v1(const char* first, const char* second, const char* playernum, const char* id)
{
    int score_win = atoi(playernum) * 25 + 150;
    int score_lose = atoi(playernum) * 25 - 250;
    int decider = rand() % 2;
    
    if (decider == 0)
    {
        // insert in file "first won against second in championship with ID : \n first got score_win points and second got score_lose points"
    }
    else
    {
        // the other way around
    }

    return decider;
}

int play_match_2v2(const char* first, const char* second, const char* third, const char* fourth, const char* playernum, const char* id)
{
    int score_win = atoi(playernum) * 15 + 250;
    int score_lose = atoi(playernum) * 15 - 350;
    int decider = rand() % 2;

    if (decider == 0)
    {

    }
    else
    {

    }

    return decider;
}