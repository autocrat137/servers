#include <sys/epoll.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <unistd.h>
#include <strings.h>
#include <sys/select.h>
#include <errno.h>
#include <arpa/inet.h>
#include <signal.h>
#include <string.h>
#include <sys/ipc.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <semaphore.h>
#include <sys/msg.h>
#include <unistd.h>
#include <sys/sem.h>
#include<sys/wait.h>
#include <time.h>

#define key 2000
#define MSG_SIZE 50

void writeToSocket(char b[30], int d, int sfd);
void recvFromSocket(int sfd, int childno);
bool isClosed(int sock);

int curr_children = 0;
int conn_no=0;

void handler(int signo){
	int status ;
	curr_children--;
	while(waitpid(-1,&status,WNOHANG)==0)
		curr_children--;
}

int main(int argc,char *argv[]) {
   if(argc!=4) {
        printf("Usage: ./a.out N M T\n");
        exit(0);
    }
    int portno;
    char ipaddr[128];
    printf("Server port number: ");
    fflush(stdout);
    scanf("%d", &portno);
    
    printf("Server ip address: ");
    fflush(stdout);
    scanf("%s", ipaddr);
    int N = atoi(argv[1]);
    int M = atoi(argv[2]);
    int T = atoi(argv[3]);
	signal(SIGCHLD,handler);
	clock_t start_time, end_time;
    int choice;
    double total_CPU_time, total_CPU_time_in_seconds;

	start_time = clock();
    for(int i=0;i<T;i++) {
		while(curr_children ==N)	//main process pauses if already N connections set
			pause();
		//if possible,make a child and connect
        if(fork() == 0) {
				conn_no++;	//new connection info for forked copy
                int childno = conn_no;
                int sfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
                struct sockaddr_in servaddr;
                servaddr.sin_family = AF_INET;
                servaddr.sin_port = htons(portno);
                servaddr.sin_addr.s_addr = inet_addr(ipaddr);
                int conret = connect(sfd, (struct sockaddr*) &servaddr, sizeof(servaddr));
                if(conret < 0)
                    perror("connect");
                writeToSocket("JOIN client%d", childno, sfd);
                
                recvFromSocket(sfd, childno);
                for(int j=0;j<M;j++) {
                    char a[MSG_SIZE];
                    sprintf(a, "UMSG client%d msg number %d from client%%d\n", childno, j);
                    // sprintf(a, "BMSG msg number %d from child%%d\n", j);
                    writeToSocket(a, childno, sfd);
    	            recvFromSocket(sfd, childno);
                }
                writeToSocket("LEAV %d", childno, sfd);
                recvFromSocket(sfd, childno);
				close(sfd);
                // pause();
				exit(0);
        }
		else{
			curr_children++;
			conn_no++;
		}
    }
	int status;
	while(wait(NULL)>0);	//collect all zombie children
	end_time = clock();
    total_CPU_time  =  (double) (end_time - start_time);
    total_CPU_time_in_seconds =   total_CPU_time / CLOCKS_PER_SEC;
    printf("\nTotal CPU time(secs) = %lf\n",total_CPU_time_in_seconds);

    return 0;
}

void writeToSocket(char b[MSG_SIZE], int d, int sfd) {
    char a[MSG_SIZE];
    sprintf(a, b, d);
    printf("sent: %s\n", a);
    int writeret = send(sfd, a, strlen(a), 0);
    if(writeret < 0)    
        perror("write");
}

void recvFromSocket(int sfd, int childno) {	//receive exactly 1 message
        char buf[MSG_SIZE];
		memset(buf,0,MSG_SIZE);
        int readret = recv(sfd, buf, MSG_SIZE,0);
        if(readret == 0) {
            printf("exiting read\n");
            exit(0);
        }
        if(readret < 0) {
            return ;
        }
        printf("recvd by %d ----> %s", childno, buf);
}
