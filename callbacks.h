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

// sqlite3 callback for SELECT
// this one pushes player names in a vector of strings to
// create players vector and allow the use of the remove_from_players
// function to more easily remove losers from the vector
static int callback2(void *ans, int argc, char **argv, char **azColName)
{
    std::string tmp(argv[0]);
    players.push_back(tmp);

    return 0;
}