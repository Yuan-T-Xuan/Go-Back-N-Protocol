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
#define LOSS_PROB 0.02    /* loss probability                            */
#define CORR_PROB 0.04    /* corruption probability                      */
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
    uint16_t checksum;        /* header and payload checksum                */
    uint8_t  type;            /* packet type (e.g. SYN, DATA, ACK, FIN)     */
    uint8_t  padding;
    int  seqnum;              /* sequence number of the packet              */
    int  bodylen;
    uint8_t data[DATALEN];    /* pointer to the payload                     */
} gbnhdr;

typedef struct {
    uint16_t checksum;        /* header and payload checksum                */
    uint8_t  type;            /* packet type (e.g. SYN, DATA, ACK, FIN)     */
    uint8_t  padding;
    int  seqnum;              /* sequence number of the packet              */
} gbnhdronly;

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
gbnhdr prevsent2;
int numtried;
int sendsecond = 0;

/* States used by receiver */
struct sockaddr *clientaddr;
socklen_t clientaddrlen;
size_t lastdatalen = 0;

/* - */

uint16_t checksum(uint16_t *buf, int nwords) {
    uint32_t sum;

    for (sum = 0; nwords > 0; nwords--)
        sum += *buf++;
    sum = (sum >> 16) + (sum & 0xffff);
    sum += (sum >> 16);
    return ~sum;
}

ssize_t maybe_sendto(int  s, const void *buf, size_t len, int flags, const struct sockaddr *to, socklen_t tolen) {
    char *buffer = malloc(len);
    memcpy(buffer, buf, len);
    /*----- Packet not lost -----*/
    if (rand() > LOSS_PROB*RAND_MAX) {
        /*----- Packet corrupted -----*/
        if (rand() < CORR_PROB*RAND_MAX) {
            /*----- Selecting a random byte inside the packet -----*/
            int index = (int)((len-1)*rand()/(RAND_MAX + 1.0));
            /*----- Inverting a bit -----*/
            char c = buffer[index];
            if (c & 0x01) c &= 0xFE;
            else c |= 0x01;
            buffer[index] = c;
        }
        /*----- Sending the packet -----*/
        int retval = sendto(s, buffer, len, flags, to, tolen);
        free(buffer);
        return retval;
    }
    /*----- Packet lost -----*/
    else
        return(len);  /* Simulate a success */
}

void handler(int signum) {
    /* handle timer alarm here */
    numtried++;
    if(numtried >= 10) {
        //printf("broken network\n");
        exit(1);
    }

    if(prevsendtype == 0) {
        printf("resending SYN/FIN ...\n");
        maybe_sendto(usingsockfd, &prevsent0, sizeof(prevsent0), 0, serveraddr, serveraddrlen);
    } else {
        printf("resending DATA pack no. %d ...\n", prevsent1.seqnum);
        maybe_sendto(usingsockfd, &prevsent1, sizeof(prevsent1), 0, serveraddr, serveraddrlen);
        // if(sendsecond) {
        //     maybe_sendto(usingsockfd, &prevsent2, sizeof(prevsent2), 0, serveraddr, serveraddrlen);
        // }
    }
    sendsecond = 0;
    signal(SIGALRM, handler);
    alarm(1);
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
    prevsent0.checksum = 0;
    //
    prevsent0.checksum = checksum(&prevsent0, sizeof(prevsent0)/2 );
    if(checksum(&prevsent0, sizeof(prevsent0)/2) != 0) {
        printf("assersion failed\n"); exit(1);
    }
    //
    beginseq = prevsent0.seqnum;
    currseq = prevsent0.seqnum;
    maybe_sendto(sockfd, &prevsent0, sizeof(prevsent0), 0, addr, addrlen);
    alarm(1);
    gbnhdronly tmpbuf;
    struct sockaddr_in si_tmp;
    int tmp_slen;
    gbnhdronly* received;
RECVAGAIN:
    recvfrom(sockfd, &tmpbuf, sizeof(tmpbuf), 0, &si_tmp, &tmp_slen);
    printf("received type: %d\n", tmpbuf.type);
    if(checksum(&tmpbuf, sizeof(tmpbuf)/2) != 0) {
        printf("checksum failed ...\n");
        goto RECVAGAIN;
    }

    received = &tmpbuf;
    if(received->type == SYNACK) {
        alarm(0);
        numtried = 0;
        return 1;
    } else {
        goto RECVAGAIN;
    }
}

int gbn_close(int s) {
    if(s != usingsockfd)
        return -1;
    prevsendtype = 0;
    prevsent0.type = FIN;
    prevsent0.seqnum = 0;
    prevsent0.checksum = 0;
    //
    prevsent0.checksum = checksum(&prevsent0, sizeof(prevsent0)/2 );
    if(checksum(&prevsent0, sizeof(prevsent0)/2) != 0) {
        printf("assersion failed (2)\n"); exit(1);
    }
    //
    currseq = prevsent0.seqnum;
    maybe_sendto(s, &prevsent0, sizeof(prevsent0), 0, serveraddr, serveraddrlen);
    alarm(1);
    gbnhdronly received;
    struct sockaddr_in si_tmp;
    int tmp_slen;
RECVAGAIN:
    recvfrom(s, &received, sizeof(received), 0, &si_tmp, &tmp_slen);
    if(checksum(&received, sizeof(received)/2) != 0) {
        printf("checksum failed ...\n");
        goto RECVAGAIN;
    }
    if(received.type == FINACK) {
        alarm(0);
        numtried = 0;
        return 1;
    } else {
        goto RECVAGAIN;
    }
}

int gbn_send(int s, char *buf, size_t len, int flags) {
    if(s != usingsockfd)
        return -1;
    /* ... */
    //char tmpbuf[DATALEN + 10];
    gbnhdronly tmpbuf;
    struct sockaddr_in si_tmp;
    int tmp_slen;
    gbnhdronly* received;
    /* ... */
    int i = 0, j, t;
    while(i < len) {
        //printf("current i : %d\n", i);
        prevsent1.type = DATA;
        prevsent1.seqnum = i + 1;
        //printf("currseq: %d\n", prevsent1.seqnum);
        prevsent1.checksum = 0;
        j = 0;
        prevsent1.bodylen = 0;
        while(j < DATALEN && (i+j) < len) {
            prevsent1.data[j] = buf[i+j];
            j++;
            prevsent1.bodylen++;
        }
        //
        prevsent1.checksum = checksum(&prevsent1, sizeof(prevsent1)/2 );
        //
        prevsendtype = 1;
        currseq = prevsent1.seqnum;
        //printf("currseq: %d ...\n", prevsent1.seqnum);
        printf("%d\n", prevsent1.seqnum);
        maybe_sendto(s, &prevsent1, sizeof(prevsent1), 0, serveraddr, serveraddrlen);
        alarm(1);
        /* send second DATA pack if in fast mode */
        if(sendsecond && i + DATALEN < len) {
            prevsent2.type = DATA;
            prevsent2.seqnum = i + DATALEN + 1;
            prevsent2.checksum = 0;
            t = 0;
            prevsent2.bodylen = 0;
            while(t < DATALEN && (i+DATALEN+t) < len) {
                prevsent2.data[t] = buf[i+DATALEN+t];
                t++;
                prevsent2.bodylen++;
            }
            prevsent2.checksum = checksum(&prevsent2, sizeof(prevsent2)/2 );
            maybe_sendto(s, &prevsent2, sizeof(prevsent2), 0, serveraddr, serveraddrlen);
        } else if(sendsecond) {
            sendsecond = 0;
        }
        /* ... */
RECVAGAIN:
        //printf("waiting here ...\n");
        recvfrom(s, &tmpbuf, sizeof(tmpbuf), 0, &si_tmp, &tmp_slen);
        //printf("got something ...\n");
        if(checksum(&tmpbuf, sizeof(tmpbuf)/2) != 0) {
            printf("checksum failed ...\n");
            goto RECVAGAIN;
        }
        received = &tmpbuf;
        printf("received DATA-ACK? ");
        //printf("type: %d\n", received->type);
        printf("seqnum is %d\n", received->seqnum);
        if(received->type == DATA) {
            //printf("strange ...\n");
            maybe_sendto(s, &prevsent1, sizeof(prevsent1), 0, serveraddr, serveraddrlen);
            goto RECVAGAIN;
        }
        else if(received->type == DATAACK) {
            //printf("got DATAACK\n");
            //printf("seqnum: %d\n\n", received->seqnum);
            if(received->seqnum == currseq /* also do checksum here */) {
                //printf("???\n");
                alarm(0);
                numtried = 0;
                i = i + j;
                sendsecond = 1;
                continue;
            } else {
                //printf("broken package, receiving again ...\n");
                goto RECVAGAIN;
            }
        } else {
            //printf("wrong package, receiving again ...\n");
            goto RECVAGAIN;
        }
    }
    return 1;
}

/*
below is only used by receiver
*/

int gbn_listen(int sockfd, int backlog) {
    /* do some server-side initialization if necessary */
    return 1;
}

int gbn_accept(int sockfd, struct sockaddr *client, socklen_t *socklen) {
    //char buf[DATALEN + 10];
    gbnhdronly tmpbuf;
RECVAGAIN:
    recvfrom(sockfd, &tmpbuf, sizeof(tmpbuf), 0, client, socklen);
    //
    if(checksum(&tmpbuf, sizeof(tmpbuf)/2) != 0) {
        printf("checksum failed ...\n");
        goto RECVAGAIN;
    }
    //
    usingsockfd = sockfd;
    clientaddr = client;
    clientaddrlen = *socklen;
    gbnhdronly* received;
    received = &tmpbuf;
    if(received->type == SYN) {
        /* TODO: do checksum here */
        beginseq = received->seqnum;
        currseq = 1;
        gbnhdronly synackpack;
        synackpack.type = SYNACK;
        synackpack.seqnum = beginseq;
        synackpack.checksum = 0;
        //
        synackpack.checksum = checksum(&synackpack, sizeof(synackpack)/2 );
        //
        maybe_sendto(sockfd, &synackpack, sizeof(synackpack), 0, clientaddr, clientaddrlen);
        //printf("accepted.\n");
        return sockfd;
    } else {
        goto RECVAGAIN;
    }
}

ssize_t gbn_recv(int sockfd, uint8_t *buf, size_t len, int flags) {
    if(sockfd != usingsockfd)
        return -1;
    //char tmpbuf[DATALEN + 10];
    gbnhdr tmpbuf;
    struct sockaddr_in si_tmp;
    int tmpsocklen;
    gbnhdr* received;
RECVAGAIN:
    recvfrom(sockfd, &tmpbuf, sizeof(tmpbuf), 0, &si_tmp, &tmpsocklen);
    received = &tmpbuf;
    printf("received type: %d\n", received->type);
    if(received->type == SYN && received->seqnum == 0) {
        if(checksum(&tmpbuf, sizeof(gbnhdronly)/2) != 0) {
            printf("checksum failed ...\n");
            goto RECVAGAIN;
        }
        gbnhdronly rsynackpack;
        rsynackpack.type = SYNACK;
        rsynackpack.seqnum = 0;
        rsynackpack.checksum = 0;
        //
        rsynackpack.checksum = checksum(&rsynackpack, sizeof(rsynackpack)/2 );
        //
        maybe_sendto(sockfd, &rsynackpack, sizeof(rsynackpack), 0, &si_tmp, tmpsocklen);
        goto RECVAGAIN;
    }
    if(received->type == FIN && received->seqnum == 0) {
        if(checksum(&tmpbuf, sizeof(gbnhdronly)/2) != 0) {
            printf("checksum failed ...\n");
            goto RECVAGAIN;
        }
        
        gbnhdronly finackpack;
        finackpack.type = FINACK;
        finackpack.seqnum = 0;
        finackpack.checksum = 0;
        //
        finackpack.checksum = checksum(&finackpack, sizeof(finackpack)/2 );
        //
        maybe_sendto(sockfd, &finackpack, sizeof(finackpack), 0, &si_tmp, tmpsocklen);
        maybe_sendto(sockfd, &finackpack, sizeof(finackpack), 0, &si_tmp, tmpsocklen);
        maybe_sendto(sockfd, &finackpack, sizeof(finackpack), 0, &si_tmp, tmpsocklen);
        maybe_sendto(sockfd, &finackpack, sizeof(finackpack), 0, &si_tmp, tmpsocklen);
        maybe_sendto(sockfd, &finackpack, sizeof(finackpack), 0, &si_tmp, tmpsocklen);
        maybe_sendto(sockfd, &finackpack, sizeof(finackpack), 0, &si_tmp, tmpsocklen);
        maybe_sendto(sockfd, &finackpack, sizeof(finackpack), 0, &si_tmp, tmpsocklen);
        maybe_sendto(sockfd, &finackpack, sizeof(finackpack), 0, &si_tmp, tmpsocklen);
        maybe_sendto(sockfd, &finackpack, sizeof(finackpack), 0, &si_tmp, tmpsocklen);
        maybe_sendto(sockfd, &finackpack, sizeof(finackpack), 0, &si_tmp, tmpsocklen);
        return 0;
    }
    if(checksum(&tmpbuf, sizeof(tmpbuf)/2) != 0) {
        printf("checksum failed ...\n");
        goto RECVAGAIN;
    }
    //printf("received!\n");
    //printf("type: %d\n", received->type);
    //printf("seqnum is %d should be ", received->seqnum);
    //printf("%d\n%d\n%d\n", currseq+lastdatalen, currseq, lastdatalen);
    //
    
    if(received->seqnum == 1 & currseq >1000000) {
        currseq = 1;
        lastdatalen = 0;
    }
    
    //
    if(received->type == DATA && received->seqnum < currseq+lastdatalen) {
        gbnhdronly ulackpack;
        ulackpack.type = DATAACK;
        ulackpack.seqnum = received->seqnum;

        ulackpack.checksum = 0;
        //
        ulackpack.checksum = checksum(&ulackpack, sizeof(ulackpack)/2 );
        //
        maybe_sendto(sockfd, &ulackpack, sizeof(ulackpack), 0, &si_tmp, tmpsocklen);
        printf("getting earlier packs again... re-recv\n");
        goto RECVAGAIN;
    } else if(received->type == DATA && received->seqnum == currseq+lastdatalen) {
        gbnhdronly dataackpack;
        dataackpack.type = DATAACK;
        dataackpack.seqnum = received->seqnum;
        printf("%d\n", received->seqnum);
        dataackpack.checksum = 0;
        //
        dataackpack.checksum = checksum(&dataackpack, sizeof(dataackpack)/2 );
        //
        maybe_sendto(sockfd, &dataackpack, sizeof(dataackpack), 0, &si_tmp, tmpsocklen);
        for(int ii = 0; ii < received->bodylen; ii++) {
            buf[ii] = received->data[ii];
        }
        currseq = received->seqnum;
        lastdatalen = received->bodylen;
        return lastdatalen;
    } else {
        //printf("wrong or broken pack... re-recv\n");
        goto RECVAGAIN;
    }
}

#endif

