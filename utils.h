int getCommandCode(char received[])
{
    if (strcmp(received, "exit") == 0)
        return 0;
    if (strcmp(received, "help") == 0)
        return 1;
    if (strcmp(received, "register") == 0)
        return 2;
    if (strcmp(received, "login") == 0)
        return 3;
    if (strcmp(received, "logout") == 0)
        return 4;
    if (strcmp(received, "add-admin") == 0)
        return 5;
    if (strcmp(received, "create-championship") == 0)
        return 6;
    if (strcmp(received, "view-championships") == 0)
        return 7;
    if (strcmp(received, "enter-championship") == 0)
        return 8;
    if (strcmp(received, "change-date") == 0)
        return 9;
    if (strcmp(received, "change-username") == 0)
        return 10;
    if (strcmp(received, "change-email") == 0)
        return 11;
    if (strcmp(received, "change-password") == 0)
        return 12;
    return -1;
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
bool check_date(char *str)
{
    if (str == nullptr)
        return 1;

    std::string s(str);

    const std::regex pattern("^(?:(?:31(\\/|-|\\.)(?:0?[13578]|1[02]|(?:Jan|Mar|May|Jul|Aug|Oct|Dec)))\\1|(?:(?:29|30)(\\/|-|\\.)(?:0?[1,3-9]|1[0-2]|(?:Jan|Mar|Apr|May|Jun|Jul|Aug|Sep|Oct|Nov|Dec))\\2))(?:(?:1[6-9]|[2-9]\\d)?\\d{2})$|^(?:29(\\/|-|\\.)(?:0?2|(?:Feb))\\3(?:(?:(?:1[6-9]|[2-9]\\d)?(?:0[48]|[2468][048]|[13579][26])|(?:(?:16|[2468][048]|[3579][26])00))))$|^(?:0?[1-9]|1\\d|2[0-8])(\\/|-|\\.)(?:(?:0?[1-9]|(?:Jan|Feb|Mar|Apr|May|Jun|Jul|Aug|Sep))|(?:1[0-2]|(?:Oct|Nov|Dec)))\\4(?:(?:1[6-9]|[2-9]\\d)?\\d{2})$");

    return std::regex_match(s, pattern);
}

// check if entered hour is formatted correctly
bool check_hour(char *str)
{
    if (str == nullptr)
        return 1;

    std::string s(str);

    const std::regex pattern("^([0-9]|0[0-9]|1[0-9]|2[0-3]):[0-5][0-9]$");

    return std::regex_match(s, pattern);
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

// remove losers from players
void remove_from_players(std::vector<std::string> losers)
{
    for (auto player_name : losers)
        players.erase(std::remove(players.begin(), players.end(), player_name), players.end());
}