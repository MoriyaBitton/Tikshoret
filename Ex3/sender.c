#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#include <unistd.h>

#define SIZE 1024

// CLEANT

int main()
{
    struct sockaddr_in dest_info;

    // ------------------------------------------------------------
    // STEP 1: Open Socket TCP
    // ------------------------------------------------------------
    int sockfd = socket(AF_INET, SOCK_STREAM /*TCP*/, 0);
    if (sockfd == -1)
    {
        printf("\t------ open socket FAILED ------\n");
        exit(0);
    }
    printf("\t------ Socket created SUCCEES ------\n");
    bzero(&dest_info, sizeof(dest_info)); /*turn &dest_info to "0" --> (need before reload)*/

    // all the data dest are reload to "dest_info"
    dest_info.sin_family = AF_INET;         /*sending type - IpV6*/
    dest_info.sin_addr.s_addr = INADDR_ANY; /*read all interfaces*/
    dest_info.sin_port = 8080;              /*protocol kind*/

    // ------------------------------------------------------------
    // STEP 2: Conection with - measure
    // ------------------------------------------------------------
    int check = connect(sockfd, (struct sockaddr *)&dest_info, sizeof(dest_info));
    if (check != 0)
    {
        printf("\t------ connection FAILED ------\n");
        exit(0);
    }
    /*check = 0 => connection success*/
    printf("\t------ connection SUCCESS ------\n");

    // ------------------------------------------------------------
    // STEP 3: Send the file 5 times
    // ------------------------------------------------------------
    for (int i = 0; i < 5; i++)
    {
        FILE *fp = fopen("data_d.txt", "rb"); /*Open FILE*/
        if (fp == NULL)
        {
            perror("\t------ Error open File ------\n");
            exit(1);
        }
        else
        {
            char data[SIZE] = {0}; /*Send FILE*/
            while (fgets(data, SIZE, fp) != NULL)
            {
                if (send(sockfd, data, sizeof(data), 0) == -1)
                {
                    perror("\t------ Error sending File ------\n");
                    exit(1);
                }
                bzero(data, SIZE);
            }

            // ------------------------------------------------------------
            // STEP 6: Close conection
            // ------------------------------------------------------------
            fclose(fp);
            fp = NULL;
        }
    }

    // sleep(10); /*To see a significant transition when changing algorithms*/

    // ---------------------------------------------------------
    // STEP 4: Changing the CC from cubic to reno and sending
    // ---------------------------------------------------------
    char buffer[256];
    strcpy(buffer, "reno");
    socklen_t len = strlen(buffer);
    check = setsockopt(sockfd, IPPROTO_TCP, TCP_CONGESTION, buffer, len); /*set socket --> from cubic to reno*/
    /*check if the set function work as a function, not result*/
    if (check != 0)
    {
        perror("\t------ Setsockopt FAILED ------\n");
        return -1;
    }

    len = sizeof(buffer);
    check = getsockopt(sockfd, IPPROTO_TCP, TCP_CONGESTION, buffer, &len);
    /*check the result of the setting*/
    if (check != 0)
    {
        perror("\t------ Getsockopt FAILED ------\n");
        return -1;
    }

    // ------------------------------------------------------------
    // STEP 5: Send the file 5 times
    // ------------------------------------------------------------
    for (int i = 0; i < 5; i++)
    {
        FILE *fp = fopen("data_d.txt", "rb"); /*Open FILE*/
        if (fp == NULL)
        {
            perror("\t------ Error open File ------\n");
            exit(1);
        }
        else
        {
            char data[SIZE] = {0}; /*Send FILE*/
            while (fgets(data, SIZE, fp) != NULL)
            {
                if (send(sockfd, data, sizeof(data), 0) == -1)
                {
                    perror("\t------ Error sending File ------\n");
                    exit(1);
                }
                bzero(data, SIZE);
            }

            // ------------------------------------------------------------
            // STEP 6: Close conection
            // ------------------------------------------------------------
            fclose(fp);
            fp = NULL;
        }
    }
    return 0;
}
