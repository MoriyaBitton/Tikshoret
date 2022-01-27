#include <arpa/inet.h>
#include <errno.h>
#include <math.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#define SIZE 1024

// SERVER

int main()
{
    int sockfd, n_sock;
    socklen_t addr_size;
    struct sockaddr_in s_addr, n_addr;
    struct timeval time_start, time_end;
    /*
    1 --> open socket 
    2 --> bind the socket with the adress
    3 --> listen through the socket while bound to the address
    4 --> accpet the packets from the socket
    5 --> recv - get the info from the packet
    6 --> close the connection
    */
    // ------------------------------------------------------------
    // STEP 1: Open Socket TCP
    // ------------------------------------------------------------
    sockfd = socket(AF_INET /*IPv_4*/, SOCK_STREAM /*TCP*/, 0);
    if (sockfd < 0)
    {
        perror("\t------- Open socket is FAILED -------\n");
        exit(1);
    }
    printf("\t------- Socket created SUCCEES -------\n");

    // Data dest reload to "s_addr"
    s_addr.sin_family = AF_INET;         /*sending type*/
    s_addr.sin_addr.s_addr = INADDR_ANY; /*read all interfaces*/
    s_addr.sin_port = 8080;              /*protocol kind*/

    /* Connect IP && Port with given socket */
    if (bind(sockfd, (struct sockaddr *)&s_addr /*IP && Port connect to*/, sizeof(s_addr)) < 0)
    {
        perror("\t------- bind is FAILED -------\n");
        exit(1);
    }
    printf("\t------- bind SUCCEES -------\n");

    // ------------------------------------------------------------
    // STEP 2: Listening
    // ------------------------------------------------------------
    if (listen(sockfd, 10) == 0)
    {
        printf("\t------ Listening SUCCEES ------\n");
    }
    else
    {
        perror("\t------ Listening FAILED ------\n");
        exit(1);
    }

    // ------------------------------------------------------------
    // STEP 3: Accept connection from - Sender
    // ------------------------------------------------------------
    addr_size = sizeof(n_addr);
    n_sock = accept(sockfd, (struct sockaddr *)&n_addr, &addr_size);

    // ---------------------------------------------------------------
    // STEP 4: Accept the file 5 times && Measurement of receipt time
    // ---------------------------------------------------------------
    gettimeofday(&time_start, NULL); /*START TIME*/
    for (int i = 0; i < 5; i++)
    {
        /*Write FILE*/
        int n_bytes = 0, counter_bytes = 0;
        char *buffer = (char *)malloc(sizeof(char) * SIZE);

        while (n_bytes < pow(SIZE, 2))
        {
            counter_bytes = recv(n_sock, buffer, SIZE, 0);
            if (counter_bytes <= 0)
            {
                break;
            }
            n_bytes += counter_bytes;
            bzero(buffer, SIZE); /*clean buffer for each run*/
        }
        free(buffer);
        printf("Size of bytes: %d\n", n_bytes);
    }
    gettimeofday(&time_end, NULL); /*END TIME*/

    // ------------------------------------------------------------
    // STEP 5: Average performance for sampled times
    // ------------------------------------------------------------
    double avgTimeCubic = (time_end.tv_sec - time_start.tv_sec) * 1000.0; /*sec --> ms*/
    avgTimeCubic += (time_end.tv_usec - time_start.tv_usec) / 1000.0;     /*us --> ms*/
    avgTimeCubic *= 0.2;

    // ------------------------------------------------------------
    // STEP 9: Print the measured times
    // ------------------------------------------------------------
    printf("Average time - cubic: %lf ms\n", avgTimeCubic);

    // ------------------------------------------------------------
    // STEP 6: Changing the cc from cubic to reno and sending
    // ------------------------------------------------------------
    char buffer[256];
    strcpy(buffer, "reno");
    socklen_t len = strlen(buffer);

    if (setsockopt(sockfd, IPPROTO_TCP, TCP_CONGESTION, buffer, len) != 0) /*setting the socket from reno to cubic*/
    {
        perror("\t------ Setsockopt FAILED ------\n");
        return -1;
    }

    len = sizeof(buffer);
    if (getsockopt(sockfd, IPPROTO_TCP, TCP_CONGESTION, buffer, &len) != 0) /*check if the socket is reno*/
    {
        perror("\t------ Getsockopt FAILED ------\n");
        return -1;
    }

    // ---------------------------------------------------------------
    // STEP 7: Accept the file 5 times && Measurement of receipt time
    // ---------------------------------------------------------------
    gettimeofday(&time_start, NULL); /*START TIME*/
    for (int i = 0; i < 5; i++)
    {
        /*Write FILE*/
        int n_bytes = 0, counter_bytes = 0;
        char *buffer = (char *)malloc(sizeof(char) * SIZE);

        while (n_bytes < pow(SIZE, 2))
        {
            counter_bytes = recv(n_sock, buffer, SIZE, 0);
            if (counter_bytes <= 0)
            {
                break;
            }
            n_bytes += counter_bytes;
            bzero(buffer, SIZE);
        }
        free(buffer);
        printf("Size of bytes: %d\n", n_bytes);
    }
    gettimeofday(&time_end, NULL); /*END TIME*/

    // ------------------------------------------------------------
    // STEP 8: Average performance for sampled times
    // ------------------------------------------------------------
    double avgTimeReno = (time_end.tv_sec - time_start.tv_sec) * 1000.0; /*sec --> ms*/
    avgTimeReno += (time_end.tv_usec - time_start.tv_usec) / 1000.0;     /*us --> ms*/
    avgTimeReno *= 0.2;

    // ------------------------------------------------------------
    // STEP 9: Print the measured times
    // ------------------------------------------------------------
    printf("Avarage time - reno: %lf ms\n", avgTimeReno);

    // ------------------------------------------------------------
    // STEP 10: Close file
    // ------------------------------------------------------------
    close(n_sock);
    return 0;
}