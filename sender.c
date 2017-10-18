#include "gbn.h"

int main(int argc, char *argv[]) {
    int sockfd;              /* socket file descriptor of the client            */
    int numRead;
    socklen_t socklen;       /* length of the socket structure sockaddr         */
    char buf[DATALEN * N];   /* buffer to send packets                       */
    struct hostent *he;      /* structure for resolving names into IP addresses */
    FILE *inputFile;         /* input file pointer                              */
    struct sockaddr_in server;

    socklen = sizeof(struct sockaddr);

    /*----- Checking arguments -----*/
    if (argc != 4){
        fprintf(stderr, "usage: sender <hostname> <port> <filename>\n");
        exit(-1);
    }
    
    /*----- Opening the input file -----*/
    if ((inputFile = fopen(argv[3], "rb")) == NULL){
        perror("fopen");
        exit(-1);
    }

    /*----- Resolving hostname to the respective IP address -----*/
    if ((he = gethostbyname(argv[1])) == NULL){
        perror("gethostbyname");
        exit(-1);
    }
    
    /*----- Opening the socket -----*/
    if ((sockfd = gbn_socket()) == -1){
        perror("gbn_socket");
        exit(-1);
    }
    
    struct sockaddr_in si_me;
    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(28596);
    si_me.sin_addr.s_addr = htonl(INADDR_ANY);
    gbn_bind(sockfd, &si_me, sizeof(si_me));

    /*--- Setting the server's parameters -----*/
    memset(&server, 0, sizeof(struct sockaddr_in));
    server.sin_family = AF_INET;
    server.sin_addr   = *(struct in_addr *)he->h_addr;
    server.sin_port   = htons(atoi(argv[2]));
    
    /*----- Connecting to the server -----*/
    if (gbn_connect(sockfd, (struct sockaddr *)&server, socklen) == -1){
        perror("gbn_connect");
        exit(-1);
    }
    
    /*
    ......
    */

    return(0);
}

