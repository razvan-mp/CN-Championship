#include <iostream>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <curl/curl.h>
#include <string>

// defines for mail sending service
#define FROM_ADDR "<cnproject112@gmail.com>"
#define CC_ADDR "<cnproject112@gmail.com>"

struct upload_status
{
    size_t bytes_read;
};

char *payload_text;

static size_t payload_source(char *ptr, size_t size, size_t nmemb, void *userp)
{
    struct upload_status *upload_ctx = (struct upload_status *)userp;
    const char *data;
    size_t room = size * nmemb;

    if ((size == 0) || (nmemb == 0) || ((size * nmemb) < 1))
    {
        return 0;
    }

    data = &payload_text[upload_ctx->bytes_read];

    if (data)
    {
        size_t len = strlen(data);
        if (room < len)
            len = room;
        memcpy(ptr, data, len);
        upload_ctx->bytes_read += len;

        return len;
    }

    return 0;
}

int send_mail_declined(const char *uname, const char *mail_addr, const char *id)
{
    std::string body;
    body += "Date: Tue, 29 Dec 2021 20:54:29 +1100\r\nTo: <";
    body += mail_addr;
    body += ">\r\nFrom: <cnproject112@gmail.com>\r\nCc: <cnproject112@gmail.com>\r\nMessage-ID: <dcd7cb36-11db-487a-9f3a-e652a9458efd@rfcpedant.championship.org>\r\nSubject: Championship notification\r\n\r\nHello, ";
    body += uname;
    body += ".\tYou have been declined participation in championship with ID ";
    body += id;
    body += " because the maximum player number has been reached.\nTry joining another championship from your account.\n";
    body += "\r\n\r\n";

    payload_text = (char *)malloc(sizeof(char *) * body.length() + 1);

    strcpy(payload_text, body.c_str());

    CURL *curl;
    CURLcode res = CURLE_OK;
    struct curl_slist *recipients = NULL;
    struct upload_status upload_ctx = {0};

    char *addr = (char *)malloc(sizeof(char *) * strlen(mail_addr) + 3);
    sprintf(addr, "<%s>", mail_addr);

    curl = curl_easy_init();
    if (curl)
    {
        curl_easy_setopt(curl, CURLOPT_URL, "stmpts://smtp.gmail.com:465");

        curl_easy_setopt(curl, CURLOPT_MAIL_FROM, FROM_ADDR);
        curl_easy_setopt(curl, CURLOPT_USERNAME, "cnproject112@gmail.com");
        curl_easy_setopt(curl, CURLOPT_PASSWORD, "[123]223");

        recipients = curl_slist_append(recipients, addr);
        recipients = curl_slist_append(recipients, CC_ADDR);
        curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipients);

        curl_easy_setopt(curl, CURLOPT_READFUNCTION, payload_source);
        curl_easy_setopt(curl, CURLOPT_READDATA, &upload_ctx);
        curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);

        res = curl_easy_perform(curl);

        if (res != CURLE_OK)
            fprintf(stderr, "[Server] curl_easy_perform() failed: %s\n",
                    curl_easy_strerror(res));

        curl_slist_free_all(recipients);
        curl_easy_cleanup(curl);
    }
    return (int)res;
}

int send_mail_1v1(const char *first_player, const char *second_player, const char *date, const char *first_player_mail, const char *second_player_mail, const char *id)
{
    std::string body;
    body += "Date: Tue, 29 Dec 2021 20:54:29 +1100\r\nTo: <";
    body += first_player_mail;
    body += ">\r\nFrom: <cnproject112@gmail.com>\r\nCc: <cnproject112@gmail.com>\r\nMessage-ID: <dcd7cb36-11db-487a-9f3a-e652a9458efd@rfcpedant.championship.org>\r\nSubject: Championship notification\r\n\r\nHello, ";
    body += first_player;
    body += ".\tYou have been accepted to the championship with ID ";
    body += id;
    body += ".\n\tYour opponent is ";
    body += second_player;
    body += "and your match date is ";
    body += date;
    body += ".\nLooking forward to seeing you play.\r\n\r\n";

    payload_text = (char *)(malloc(sizeof(char *) * body.length() + 1));

    strcpy(payload_text, body.c_str());

    CURL *curl;
    CURLcode res = CURLE_OK;
    struct curl_slist *recipients = NULL;
    struct upload_status upload_ctx = {0};

    char *email = (char *)(malloc(sizeof(char *) * strlen(first_player_mail) + 3));
    sprintf(email, "<%s>", first_player_mail);

    curl = curl_easy_init();
    if (curl)
    {
        // mailserver URL
        curl_easy_setopt(curl, CURLOPT_URL, "smtps://smtp.gmail.com:465");

        // specifying uname and pass in code since it's a local app and
        // security is not really a concern
        curl_easy_setopt(curl, CURLOPT_MAIL_FROM, FROM_ADDR);
        curl_easy_setopt(curl, CURLOPT_USERNAME, "cnproject112@gmail.com");
        curl_easy_setopt(curl, CURLOPT_PASSWORD, "[123]223");

        // adding recipient to list and adding default e-mail
        // address so verification can be made
        recipients = curl_slist_append(recipients, email);
        recipients = curl_slist_append(recipients, CC_ADDR);
        curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipients);

        curl_easy_setopt(curl, CURLOPT_READFUNCTION, payload_source);
        curl_easy_setopt(curl, CURLOPT_READDATA, &upload_ctx);
        curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);

        res = curl_easy_perform(curl);

        if (res != CURLE_OK)
            fprintf(stderr, "[Server] curl_easy_perform() failed: %s\n",
                    curl_easy_strerror(res));

        // clearing recipient list and cleanup
        // since curl doesn't automatically do
        // it after sending an e-mail
        curl_slist_free_all(recipients);
        curl_easy_cleanup(curl);
    }
    return (int)res;
}

int send_mail_2v2(const char *user, const char *teammate, const char *opp_1, const char *opp_2, const char *player_mail, const char *date, const char *champ_id)
{
    std::string body;

    body += "Date: Tue, 29 Dec 2021 20:54:29 +1100\r\nTo: <";
    body += player_mail;
    body += ">\r\nFrom: <cnproject112@gmail.com>\r\nCc: <cnproject112@gmail.com>\r\nMessage-ID: <dcd7cb36-11db-487a-9f3a-e652a9458efd@rfcpedant.championship.org>\r\nSubject: Championship notification\r\n\r\nHello, ";
    body += user;
    body += ".\tYou have been accepted to the championship with ID ";
    body += champ_id;
    body += ".\n\tYour teammate is ";
    body += teammate;
    body += "and your opponents are ";
    body += opp_1;
    body += " and ";
    body += opp_2;
    body += ".\nYour match is set for ";
    body += date;
    body += ".\nEnter your account and enter 'change-date' if you're unavailable to play. But if it's all good, we're looking forward to seeing you play.\r\n\r\n";

    payload_text = (char *)(malloc(sizeof(char *) * body.length() + 1));

    strcpy(payload_text, body.c_str());

    CURL *curl;
    CURLcode res = CURLE_OK;
    struct curl_slist *recipients = NULL;
    struct upload_status upload_ctx = {0};

    char *email = (char *)(malloc(sizeof(char *) * strlen(player_mail) + 3));
    sprintf(email, "<%s>", player_mail);

    curl = curl_easy_init();
    if (curl)
    {
        // mailserver URL
        curl_easy_setopt(curl, CURLOPT_URL, "smtps://smtp.gmail.com:465");

        // specifying uname and pass in code since it's a local app and
        // security is not really a concern
        curl_easy_setopt(curl, CURLOPT_MAIL_FROM, FROM_ADDR);
        curl_easy_setopt(curl, CURLOPT_USERNAME, "cnproject112@gmail.com");
        curl_easy_setopt(curl, CURLOPT_PASSWORD, "[123]223");

        // adding recipient to list and adding default e-mail
        // address so verification can be made
        recipients = curl_slist_append(recipients, email);
        recipients = curl_slist_append(recipients, CC_ADDR);
        curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipients);

        curl_easy_setopt(curl, CURLOPT_READFUNCTION, payload_source);
        curl_easy_setopt(curl, CURLOPT_READDATA, &upload_ctx);
        curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);

        res = curl_easy_perform(curl);

        if (res != CURLE_OK)
            fprintf(stderr, "[Server] curl_easy_perform() failed: %s\n",
                    curl_easy_strerror(res));

        // clearing recipient list and cleanup
        // since curl doesn't automatically do
        // it after sending an e-mail
        curl_slist_free_all(recipients);
        curl_easy_cleanup(curl);
    }
    return (int)res;
}
