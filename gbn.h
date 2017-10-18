#ifndef GBN_H
#define GBN_H

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <sys/ioctl.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <netdb.h>
#include <time.h>

/*----- Error variables -----*/
extern int h_errno;
extern int errno;

/*----- Protocol parameters -----*/
#define LOSS_PROB 1e-2    /* loss probability                            */
#define CORR_PROB 1e-3    /* corruption probability                      */
#define DATALEN   1024    /* length of the payload                       */
#define N         1024    /* Max number of packets a single call to gbn_send can process */
#define TIMEOUT      1    /* timeout to resend packets (1 second)        */

/*----- Packet types -----*/
#define SYN      0        /* Opens a connection                          */
#define SYNACK   1        /* Acknowledgement of the SYN packet           */
#define DATA     2        /* Data packets                                */
#define DATAACK  3        /* Acknowledgement of the DATA packet          */
#define FIN      4        /* Ends a connection                           */
#define FINACK   5        /* Acknowledgement of the FIN packet           */
#define RST      6        /* Reset packet used to reject new connections */

/* for backward compatibility */
#define h_addr h_addr_list[0]

typedef struct {
    uint8_t  type;            /* packet type (e.g. SYN, DATA, ACK, FIN)     */
    uint8_t  seqnum;          /* sequence number of the packet              */
    uint16_t checksum;        /* header and payload checksum                */
    uint8_t data[DATALEN];    /* pointer to the payload                     */
} __attribute__((packed)) gbnhdr;

typedef struct {
    uint8_t  type;            /* packet type (e.g. SYN, DATA, ACK, FIN)     */
    uint8_t  seqnum;          /* sequence number of the packet              */
    uint16_t checksum;        /* header and payload checksum                */
} __attribute__((packed)) gbnhdronly; // should I remove "packed"?

/* Shared states (but used somehow differently) */
int beginseq;
int currseq;
int usingsockfd;

/* States used by sender */
struct sockaddr *serveraddr;
socklen_t serveraddrlen;
int prevsendtype;             /* 0 for hdronly */
gbnhdronly prevsent0;
gbnhdr prevsent1;
int numtried;

/* States used by receiver */
struct sockaddr *clientaddr;
socklen_t clientaddrlen;

/* - */

void handler(int signum) {
    /* handle timer alarm here */
    printf("resending ...\n");
    numtried++;
    if(numtried >= 10) {
        printf("broken network\n");
        exit(1);
    }

    if(prevsendtype == 0) {
        sendto(usingsockfd, &prevsent0, sizeof(prevsent0), 0, serveraddr, serveraddrlen);
        signal(SIGALRM, handler);
        alarm(2);
    } else {
        return;
    }
}

int gbn_socket() {
    srand((unsigned)time(0));
    return socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
}

int gbn_bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    return bind(sockfd, addr, addrlen);
}

int gbn_connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    signal(SIGALRM, handler);
    usingsockfd = sockfd;
    serveraddr = addr;
    serveraddrlen = addrlen;
    prevsendtype = 0;
    prevsent0.type = SYN;
    prevsent0.seqnum = 0;
    /* prevsent0.checksum = */ prevsent0.checksum = 0;
    beginseq = prevsent0.seqnum;
    currseq = prevsent0.seqnum;
    sendto(sockfd, &prevsent0, sizeof(prevsent0), 0, addr, addrlen);
    alarm(2);
    char buf[DATALEN + 10];
    struct sockaddr_in si_tmp;
    int tmp_slen;
    gbnhdronly* received;
RECVAGAIN:
    recvfrom(sockfd, buf, DATALEN + 10, 0, &si_tmp, &tmp_slen);
    
    received = buf;
    if(received->type == SYNACK) {
        return 1;
    } else {
        printf("wrong package, receiving again ...\n");
        goto RECVAGAIN;
    }
}

/*
below is only used by receiver
*/

int gbn_listen(int sockfd, int backlog) {
    /* do some server-side initialization if necessary */
    return 1;
}

int gbn_accept(int sockfd, struct sockaddr *client, socklen_t *socklen) {
    char buf[DATALEN + 10];
RECVAGAIN:
    recvfrom(sockfd, buf, DATALEN + 10, 0, client, socklen);
    usingsockfd = sockfd;
    clientaddr = client;
    clientaddrlen = *socklen;
    gbnhdronly* received;
    received = buf;
    if(received->type == SYN) {
        /* TODO: do checksum here */
        beginseq = received->seqnum;
        currseq = received->seqnum;
        gbnhdronly synackpack;
        synackpack.type = SYNACK;
        synackpack.seqnum = beginseq;
        /* synackpack.checksum = */ synackpack.checksum = 0;
        sendto(sockfd, &synackpack, sizeof(synackpack), 0, clientaddr, clientaddrlen);
        printf("accepted.\n");
    } else {
        goto RECVAGAIN;
    }
}

#endif

