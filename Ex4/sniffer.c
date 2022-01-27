// --------------------------------------------------------------------
// sniffer.c
//
// Sniffer message using ICMP protocols.
// -------------------------------------------------------------------- 

#include <linux/if_ether.h>
#include <netinet/ip_icmp.h>
#include <pcap.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>

// --------------------------------------------------------------------
// Extracts information from the received package
// --------------------------------------------------------------------
void got_packet(u_char *args, const struct pcap_pkthdr *header, const u_char *packet)
{
    /*
    1 --> arguments we want to pass to the function ?
    2 --> packet header, hold the information in packet 
    3 --> the packet itself
    */
    printf("Got ICMP Message:\n");

    /* ip header */
    struct iphdr *ip = (struct iphdr *)(packet + sizeof(struct ethhdr));

    /* source, dest of meassege */
    struct sockaddr_in source, dest;
    source.sin_addr.s_addr = ip->saddr;
    dest.sin_addr.s_addr = ip->daddr;

    /* Function inet_ntoa: 
    converts the Internet host address from the IPv4 
    numbers-and-dots into binary form. 
    The string is returned in a statically allocated buffer,
    which subsequent calls will overwrite.
    returns nonzero if the address is valid, zero if not. */
    char *sourceIP = inet_ntoa(source.sin_addr);
    printf("Source IP: %s\n", sourceIP);
    char *destIP = inet_ntoa(dest.sin_addr);
    printf("Destination IP: %s\n", destIP);

    /* icmp header */
    struct icmphdr *icmp = (struct icmphdr *)(packet + sizeof(struct ethhdr) + sizeof(struct iphdr));

    printf("ICMP type: %d\n", icmp->type);
    printf("ICMP code: %d\n\n", icmp->code);
}

int main()
{
    pcap_t *handle;                /* Session handle --> handle of the device that shold be sniffed */
    char errbuf[PCAP_ERRBUF_SIZE]; /* Error string --> handle of the device that shold be sniffed */
    struct bpf_program fp;         /* The compiled filter expression */
    char filter[] = "icmp";        /* The filter expression */
    bpf_u_int32 net;               /* The IP of our sniffing device */

    /* Step 1: Open live pcap session on NIC with name enp0s3 - name of the configurble network interface */
    handle = pcap_open_live("enp0s3", BUFSIZ, 1, 1000, errbuf);
    /*
    1 --> NIC interface
    2 --> maximum packet size
    3 --> promiscious mode or not
    5 --> save error in buff
    */
    if (handle == NULL)
    {
        fprintf(stderr, "%s\n", errbuf);
        exit(1);
    }

    /* Step 2: Compile filter into BPF psuedo-code */
    pcap_compile(handle, &fp, filter, 0, net); /* Checking Syntax */
    if (pcap_setfilter(handle, &fp) == -1) /* Checking if the set of the filter to ICMP worked */
    {
        pcap_perror(handle, "failed to set filter"); /* print error and exit */
        exit(1);
    }
 
    /* Step 3: Capture packets */
    pcap_loop(handle, -1, got_packet, NULL); 
    /*
    1 --> session handler
    2 --> amount of packet we are going to sniff || (-1) == infinity
    */

    /* Close the handle */
    pcap_close(handle);
    return 0;
}