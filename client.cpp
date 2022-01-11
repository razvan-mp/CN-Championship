#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sqlite3.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>

int main()
{
    int sd;                    // descriptorul de socket
    struct sockaddr_in server; // structura folosita pentru conectare
    char msg[5000];            // mesajul trimis
    char received[5000];

    // create socket
    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("[Client] Error on socket().\n");
        return 1;
    }

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr("127.0.0.1");
    server.sin_port = htons(2728);

    if (connect(sd, (struct sockaddr *)&server, sizeof(struct sockaddr)) == -1)
    {
        perror("[Client] Error on connect().\n");
        return 1;
    }

    if (read(sd, received, 5000) < 0)
    {
        perror("[Client] Error on read() from server.\n");
        return 1;
    }
    printf("%s", received);
    fflush(stdout);

    while (true)
    {
        bzero(msg, 5000);
        read(0, msg, 5000);

        if (strcmp(msg, "exit\n") == 0)
        {
            write(sd, msg, 5000);
            return 1;
        }

        if (strcmp(msg, "\n") == 0)
        {
            write(sd, " ", 1);
        }
        else
        {
            if (write(sd, msg, strlen(msg) - 1) <= 0)
            {
                perror("[Client] Error on write() to the server.\n");
                return 1;
            }
        }

        bzero(msg, 5000);
        if (read(sd, msg, 5000) < 0)
        {
            perror("[Client] Error on read() from server.\n");
            return 1;
        }

        printf("%s", msg);
        fflush(stdout);
    }
}
