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

//extern int errno;

int port;

int main(int argc, char *argv[])
{
    int sd;                    // descriptorul de socket
    struct sockaddr_in server; // structura folosita pentru conectare
    char msg[1000];             // mesajul trimis
    char received[1000];

    /* exista toate argumentele in linia de comanda? */
    if (argc != 3)
    {
        printf("[client] Syntax: %s <ip_address> <port>\n", argv[0]);
        return -1;
    }

    /* stabilim portul */
    port = atoi(argv[2]);

    /* cream socketul */
    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("[client] Eroare la socket().\n");
        return errno;
    }

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(argv[1]);
    server.sin_port = htons(port);

    if (connect(sd, (struct sockaddr *)&server, sizeof(struct sockaddr)) == -1)
    {
        perror("[Client] Error on connect().\n");
        return errno;
    }

    if (read(sd, received, 1000) < 0)
    {
        perror("[Client] Error on read() from server.\n");
        return 1;
    }
    printf("%s", received);
    fflush(stdout);

    while (true)
    {
        bzero(msg, 1000);
        read(0, msg, 1000);

        if (strncmp(msg, "exit", 4) == 0)
        {
            write(sd, msg, 1000);
            return 1;
        }

        if (write(sd, msg, strlen(msg) - 1) <= 0)
        {
            perror("[Client] Error on write() to the server.\n");
            return errno;
        }
        else
        {
            printf("[Client] Message sent to server successfully.\n");
            fflush(stdout);
        }

        bzero(msg, 1000);
        if (read(sd, msg, 1000) < 0)
        {
            perror("[Client] Error on read() from server.\n");
            return errno;
        }
        else
        {
            printf("[Client] Read from server successfully.\n");
            fflush(stdout);
        }
        printf("%s", msg);
        fflush(stdout);
    }
}