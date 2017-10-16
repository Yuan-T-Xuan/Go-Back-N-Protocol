#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>

void handler(int signum) {
    printf("???\n");
    //exit(1);
}

int main() {
    printf("hello, world\n");
    signal(SIGALRM, handler);
    alarm(4);
    while(1) {
        printf("...\n");
    }
    printf("nonono\n");
    return 0;
}
