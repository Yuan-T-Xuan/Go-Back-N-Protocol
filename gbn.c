#include "gbn.h"

int currstate = 0; /* 0: off, 1: connected, 2: connected (receiver side), */
int speedmode = 0; /* 0 for "slow" */

int numtried = 0;
int prevtype = 0; /* 0: gbnhdronly, 1: gbnhdronly */
gbnhdronly prevhdronly;
gbnhdr prevhdr;
int prevsockfd;
struct sockaddr *prevserver;
socklen_t prevsocklen;

int synseq;
int seqnum = -1;
void handler(int signum) {
    /* handle timer alarm here */
    printf("resending ...\n");
    numtried++;
    if(numtried >= 10) {
        printf("broken network\n");
        exit(1);
    }

    if(prevtype == 0) {
        sendto(prevsockfd, &prevhdronly, sizeof(gbnhdronly), 0, prevserver, prevsocklen);
        signal(SIGALRM, handler);
        alarm(1);
    } else {
        return;
    }
}



uint16_t checksum(uint16_t *buf, int nwords)
{
    uint32_t sum;

    for (sum = 0; nwords > 0; nwords--)
        sum += *buf++;
    sum = (sum >> 16) + (sum & 0xffff);
    sum += (sum >> 16);
    return ~sum;
}

ssize_t gbn_send(int sockfd, const void *buf, size_t len, int flags){
    
    /* TODO: Your code here. */

    /* Hint: Check the data length field 'len'.
     *       If it is > DATALEN, you will have to split the data
     *       up into multiple packets - you don't have to worry
     *       about getting more than N * DATALEN.
     */

    return(-1);
}

ssize_t gbn_recv(int sockfd, void *buf, size_t len, int flags){

    /* TODO: Your code here. */

    return(-1);
}

int gbn_close(int sockfd){

    /* TODO: Your code here. */

    return(-1);
}

int gbn_connect(int sockfd, const struct sockaddr *server, socklen_t socklen){
    /*
    uint8_t  type;
    uint8_t  seqnum;
    uint16_t checksum;
    */
    gbnhdronly synpack;
    synpack.type = SYN;
    synseq = 0;
    synpack.seqnum = synseq;
    seqnum = synpack.seqnum;
    synpack.checksum = checksum(&synpack, 1);
    
    prevhdronly = synpack;
    prevserver = server;
    prevsocklen = socklen;
    prevsockfd = sockfd;
    
    sendto(sockfd, &synpack, sizeof(gbnhdronly), 0, server, socklen);
    alarm(1);
    gbnhdronly synackpack;
RCVAGAIN:
    recvfrom(sockfd, &synackpack, sizeof(gbnhdronly), 0, server, socklen);
    if(synackpack.type == SYNACK) {
        alarm(0);
        currstate = 1;
        return 1;
    }
    goto RCVAGAIN;
}

int gbn_listen(int sockfd, int backlog){

    /* TODO: */
    return 0;
    /*return(-1);*/
}

int gbn_bind(int sockfd, const struct sockaddr *server, socklen_t socklen){
    return bind(sockfd, server, socklen);
}

int gbn_socket(int domain, int type, int protocol){
        
    /*----- Randomizing the seed. This is used by the rand() function -----*/
    srand((unsigned)time(0));
    
    signal(SIGALRM, handler);
    return socket(domain, type, protocol);
    
}

int gbn_accept(int sockfd, struct sockaddr *client, socklen_t *socklen) {
    gbnhdronly synpack, synackpack;
RCVAGAIN:
    recvfrom(sockfd, &synpack, sizeof(gbnhdronly), 0, client, socklen);
    if(synpack.type == SYN) {
        currstate = 2;
        synseq = synpack.seqnum;
        seqnum = synseq;
        /* send back SYNACK */
        synackpack.type = SYNACK;
        synackpack.seqnum = seqnum;
        synackpack.checksum = 0; /* TODO... */
        sendto(sockfd, &synackpack, sizeof(gbnhdronly), 0, client, socklen);
        return sockfd;
    }
    goto RCVAGAIN;
}

ssize_t maybe_sendto(int  s, const void *buf, size_t len, int flags, \
                     const struct sockaddr *to, socklen_t tolen){

    char *buffer = malloc(len);
    memcpy(buffer, buf, len);
    
    
    /*----- Packet not lost -----*/
    if (rand() > LOSS_PROB*RAND_MAX){
        /*----- Packet corrupted -----*/
        if (rand() < CORR_PROB*RAND_MAX){
            
            /*----- Selecting a random byte inside the packet -----*/
            int index = (int)((len-1)*rand()/(RAND_MAX + 1.0));

            /*----- Inverting a bit -----*/
            char c = buffer[index];
            if (c & 0x01)
                c &= 0xFE;
            else
                c |= 0x01;
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
