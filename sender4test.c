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

/*----- Packet types -----*/
#define SYN      0        /* Opens a connection                          */
#define SYNACK   1        /* Acknowledgement of the SYN packet           */
#define DATA     2        /* Data packets                                */
#define DATAACK  3        /* Acknowledgement of the DATA packet          */
#define FIN      4        /* Ends a connection                           */
#define FINACK   5        /* Acknowledgement of the FIN packet           */
#define RST      6        /* Reset packet used to reject new connections */

typedef struct {
    uint16_t checksum;        /* header and payload checksum                */
    uint8_t  type;            /* packet type (e.g. SYN, DATA, ACK, FIN)     */
    uint8_t  padding;
    uint8_t data[1024];       /* pointer to the payload                     */
    int  seqnum;              /* sequence number of the packet              */
    int  bodylen;
} gbnhdr;

typedef struct {
    uint16_t checksum;        /* header and payload checksum                */
    uint8_t  type;            /* packet type (e.g. SYN, DATA, ACK, FIN)     */
    uint8_t  padding;
    int  seqnum;              /* sequence number of the packet              */
} gbnhdronly;

uint16_t checksum(uint16_t *buf, int nwords) {
    uint32_t sum;

    for (sum = 0; nwords > 0; nwords--)
        sum += *buf++;
    sum = (sum >> 16) + (sum & 0xffff);
    sum += (sum >> 16);
    return ~sum;
}

int main() {
    struct sockaddr_in si_me, si_other;
    memset((char *) &si_other, 0, sizeof(si_other));
    si_other.sin_family = AF_INET;
    si_other.sin_port = htons(26993);
    if (inet_aton("127.0.0.1", &si_other.sin_addr)==0) {
        fprintf(stderr, "inet_aton() failed\n");
        exit(1);
    }
    FILE *inputFile;
    inputFile = fopen("lakes.jpg", "rb");

    gbnhdr sendpack1;
    gbnhdronly sendpack0;
    int s;
    s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    sendpack0.type = SYN;
    sendpack0.seqnum = 0;
    sendpack0.checksum = 0;
    sendpack0.checksum = checksum(&sendpack0, sizeof(sendpack0)/2 );
    sendto(s, &sendpack0, sizeof(sendpack0), 0, &si_other, sizeof(si_other));
    int numRead = 0;
    int currseq = 1;
    while((numRead = fread(sendpack1.data, 1, 1024, inputFile)) > 0) {
        //fwrite(buf, 1, numRead, outputFile);
        sendpack1.type = DATA;
        sendpack1.seqnum = currseq;
        sendpack1.bodylen = numRead;
        sendpack1.checksum = 0;
        sendpack1.checksum = checksum(&sendpack1, sizeof(sendpack1)/2 );
        sendto(s, &sendpack1, sizeof(sendpack1), 0, &si_other, sizeof(si_other));
        sendto(s, &sendpack1, sizeof(sendpack1), 0, &si_other, sizeof(si_other));
        sendto(s, &sendpack1, sizeof(sendpack1), 0, &si_other, sizeof(si_other));
        currseq += numRead;
    }
    sendpack0.type = FIN;
    sendpack0.seqnum = 0;
    sendpack0.checksum = 0;
    sendpack0.checksum = checksum(&sendpack0, sizeof(sendpack0)/2 );
    sendto(s, &sendpack0, sizeof(sendpack0), 0, &si_other, sizeof(si_other));
    sendto(s, &sendpack0, sizeof(sendpack0), 0, &si_other, sizeof(si_other));
    sendto(s, &sendpack0, sizeof(sendpack0), 0, &si_other, sizeof(si_other));
    fclose(inputFile);

    return 0;
}


