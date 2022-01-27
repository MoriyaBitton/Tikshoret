// --------------------------------------------------------------------
// myping.c
//
// Use the ICMP protocol for:
// send ICMP ECHO REQUEST, and receives ICMP-ECHO-REPLY
// --------------------------------------------------------------------

#include <netdb.h>
#include <netinet/in.h>
#include <netinet/ip_icmp.h>
#include <time.h>
#include <stdio.h>
#include <strings.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <unistd.h> /* for close */

#define ICMP_ECHO 8 /*ping not pong*/
#define IP_DEST "8.8.8.8"
#define PACKETSIZE 64

// --------------------------------------------------------------------
// ping packet structure
// --------------------------------------------------------------------
struct packet
{
    struct icmphdr hdr;
    char msg[PACKETSIZE - sizeof(struct icmphdr)];
};

struct protoent *proto = NULL;

// --------------------------------------------------------------------
// CHECKSUM - Used to check for errors by calculating the bits
// --------------------------------------------------------------------
unsigned short checksum(void *b, int len)
{
    unsigned short *buf = b;
    unsigned int sum = 0;
    unsigned short result;

    for (sum = 0; len > 1; len -= 2)
        sum += *buf++;
    if (len == 1)
        sum += *(unsigned char *)buf;
    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    result = ~sum;
    return result;
}

// --------------------------------------------------------------------
// PING - Create a message and send it
// --------------------------------------------------------------------
void ping(struct sockaddr_in *addr)
{
    int i, sd;
    struct packet pckt; /*packet*/
    struct sockaddr_in r_addr;
    struct timeval time_start, time_end;

    /* Create socket */
    sd = socket(PF_INET /*IPv*/, SOCK_RAW /*raw s for icmp protocol*/, IPPROTO_ICMP /*protocol type*/);
    // Note: raw socket requires superuser rights so you have to run this code using sudo
    if (sd < 0)
    {
        perror("socket");
        return;
    }

    bzero(&pckt, sizeof(pckt)); /*All bytes of &pckt are now 0*/

    pckt.hdr.type = ICMP_ECHO;      /*Set packet header type to ICMP_ECHO*/
    pckt.hdr.un.echo.id = getpid(); /*get the current process id (the myPing program)*/
    for (i = 0; i < sizeof(pckt.msg) - 1; i++)
    {
        pckt.msg[i] = i + '0';
    }

    pckt.msg[i] = 0;
    pckt.hdr.un.echo.sequence = 0;
    pckt.hdr.checksum = checksum(&pckt, sizeof(pckt));

    gettimeofday(&time_start, NULL); /* Start time */

    if (sendto(sd, &pckt, sizeof(pckt), 0, (struct sockaddr *)addr, sizeof(*addr)) <= 0)
    /*
    1 --> socket 
    2 && 3 --> the ICMP packet
    4 && 5 --> socket adress && len
    */
    {
        perror("sendto");
    }

    int byts = 0;
    while (byts <= 0)
    {
        /* Keep try to recive */
        socklen_t len = sizeof(r_addr);
        byts = recvfrom(sd, &pckt, sizeof(pckt), 0, (struct sockaddr *)&r_addr, &len);
        /*
        1 --> socket
        2 && 3 --> for the packet
        4 --> is flag (0 for default)
        5 && 6 --> for the address
        */
    }
    if (byts > 0)
    {
        /* Recive message */
        printf("\t\tGot message!\n");
    }

    gettimeofday(&time_end, NULL); /* End time */

    /* Calculate time */
    double milliseconds, microseconds;
    /* micro(usec) to milli --> 1 Milliseconds = 1000 Microseconds */
    microseconds = (time_end.tv_usec - time_start.tv_usec);
    milliseconds = microseconds / 1000.0;
    printf("RTT in Milliseconds: %lf\n", milliseconds);
    printf("RTT in Microseconds: %lf\n", microseconds);

    close(sd);
}

// --------------------------------------------------------------------
// MAIN - Look for the host and start pinging processes
// --------------------------------------------------------------------
int main(int count, char *strings[])
{
    struct hostent *hname;
    struct sockaddr_in addr;

    proto = getprotobyname("ICMP");
    hname = gethostbyname(IP_DEST); /*host ip*/
    bzero(&addr, sizeof(addr));     /*All bytes of &addr are now 0*/
    addr.sin_family = hname->h_addrtype;
    addr.sin_port = 0;
    addr.sin_addr.s_addr = *(long *)hname->h_addr;
    ping(&addr);
    return 0;
}