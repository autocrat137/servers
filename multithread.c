#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <strings.h>
#include <pthread.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include<sys/select.h>
#include <pthread.h>

#define MSG_SIZE 50
#define MAX_EVENTS 10
#define MAX_EVENTS 10
#define CLIENT_NAME 40

struct client{
	int fd;
	char name[CLIENT_NAME];
	struct client *next;
};

typedef struct client *Client;
Client head;

void processMessages(int self_fd);
Client addClient(Client head,char *name,int fd, char retmsg[100]);
Client findClientbyName(Client head,char *name);
Client newClient(int fd,char *name);
Client deleteClientbyfd(Client head,int fd, char retmsg[100]);
Client findClientbyfd(Client head,int fd);
static void	*serverRun(void * lfd);		/* each thread executes this function */
int writers = 0;
int writing = 0;
int reading = 0;
pthread_mutex_t mutex;
pthread_cond_t cond;

int main(int argc, char **argv) {
	socklen_t addrlen, len;
	pthread_t tid1;
	struct sockaddr_in servaddr, cliaddr;
    int *listenfd = (int *) malloc (sizeof(int));
	*listenfd = socket(AF_INET,SOCK_STREAM,0);
	bzero(&servaddr,sizeof(servaddr));
	servaddr.sin_family=AF_INET;
	servaddr.sin_addr.s_addr=htonl(INADDR_ANY);
    int port;
	printf("Enter port no: ");
    fflush(stdout);
    scanf("%d", &port);
	servaddr.sin_port=htons(port);

	int bindret = bind(*listenfd,(struct sockaddr *)&servaddr,sizeof(servaddr));
    if(bindret < 0) {
        perror("bind");
        exit(0);
    }
	if(listen(*listenfd,100) < 0) {
        perror("listen");
        exit(0);
    }

	fd_set rset, allset;
	FD_ZERO (&allset);
	FD_SET (*listenfd, &allset);
	for ( ; ; ) {
		rset = allset;
        int ready = select(*listenfd + 1, &rset, NULL, NULL, NULL);
		if (FD_ISSET (*listenfd, &rset)) {
            pthread_create(&tid1, NULL, &serverRun, listenfd);
        }
	}
}

static void *serverRun(void * lfd)
{
	struct sockaddr_in cliaddr;
	pthread_detach(pthread_self());
    int clilen = sizeof(cliaddr);
	int connfd = accept(*((int*)lfd), (struct sockaddr * ) &cliaddr,  &clilen);
    if(connfd < 0) {
        perror("accept");
        exit(0);
    }
    processMessages(connfd);
}


void processMessages(int self_fd){
	char buf[MSG_SIZE];
    int w;
	Client temp;
	while(1) { 
		memset(buf,0,MSG_SIZE);
		int r = read(self_fd,buf,MSG_SIZE);
		if(r < 0)
			perror("read error");
		if(r == 0){
			printf("FIN Received by %d\n",self_fd);
			close(self_fd);
			head = deleteClientbyfd(head, self_fd, buf);
			pthread_exit(NULL);
		}
		int i=0;
        char tempbuf[5];
		strncpy(tempbuf,buf,4);
		tempbuf[4]='\0';
		

        //WRITER
        if(strcmp(tempbuf,"JOIN")==0 || strcmp(tempbuf,"LEAV")==0) {
            pthread_mutex_lock(&mutex);
            writers++;
            while(reading || writing)
                pthread_cond_wait(&cond, &mutex);
            writing++;
            pthread_mutex_unlock(&mutex);
            if(strcmp(tempbuf,"JOIN")==0){
                char name[CLIENT_NAME];
                sscanf(buf, "%s %s", tempbuf, name);
                memset(buf,0,MSG_SIZE);
                head = addClient(head,name,self_fd,buf);	
                int len = strlen(buf);
                buf[len] = '\n';
                buf[len+1] = '\0';
                int w = write(self_fd, buf, strlen(buf));	//write to socket
                if(w < 0)
                    perror("socket write");
            }
            else if(strcmp(tempbuf,"LEAV")==0) {
                if(findClientbyfd(head, self_fd) == NULL) {
                    memset(buf,0,MSG_SIZE);
                    strcpy(buf, "You are not online");
                }
                else{
                    memset(buf,0,MSG_SIZE);
                    head = deleteClientbyfd(head, self_fd, buf);
                }
                int len = strlen(buf);
                buf[len] = '\n';
                buf[len+1] = '\0';
                int w = write(self_fd, buf, strlen(buf));	//write to socket
                if(w < 0)
                    perror("socket write");
            }
            pthread_mutex_lock(&mutex);
            writing--;
            writers--;
            pthread_mutex_unlock(&mutex);
            pthread_cond_broadcast(&cond);
        }
        
        //WRITER END

		//READER	
		else{
			pthread_mutex_lock(&mutex);
			while (writers)
            pthread_cond_wait(&cond, &mutex);
			// No need to wait while(writing here) because we can only exit the above loop
			// when writing is zero
			reading++;
			pthread_mutex_unlock(&mutex);

			// perform reading here

		
			if(strcmp(tempbuf,"LIST")==0){
				memset(buf,0,MSG_SIZE);
				strcpy(buf, "");
				sprintf(buf,"List of all online users:\n");
				temp=head;
				char toAdd[100];
				while(temp!=NULL){
					sprintf(toAdd, "%s\t", temp->name);
					strcat(buf,toAdd);
					temp=temp->next;
				}
				printf("Retrieving the list of all online users\n");
				int len = strlen(buf);
				buf[len] = '\n';
				buf[len+1] = '\0';
				int w = write(self_fd, buf, strlen(buf));
				if(w < 0)
					perror("socket write");
			}
			else if(strcmp(tempbuf,"UMSG")==0){
				Client sender = findClientbyfd(head, self_fd);
				char tname[CLIENT_NAME], umsg[CLIENT_NAME];
				if(sender==NULL){	//send a msg to itself
					printf("Client not online\n");
					memset(buf,0,MSG_SIZE);
					sprintf(buf, "You are not online");
					int len = strlen(buf);
					buf[len] = '\n';
					buf[len+1] = '\0';
					w = write(self_fd, buf, strlen(buf));
					if(w < 0)
						perror("socket write");
				}
				else{
					sscanf(buf, "%s %s %[^\n]", tempbuf, tname, umsg);
					memset(buf,0,MSG_SIZE);
					sprintf(buf,"[%s]->",sender->name);
					strcat(buf, umsg);
					Client cli = findClientbyName(head,tname);
					if(cli==NULL){	//send to itself
						printf("Client %s is not online\n",tname);
						sprintf(buf, "Client %s is not online", tname);
						int len = strlen(buf);
						buf[len] = '\n';
						buf[len+1] = '\0';
						w = write(self_fd, buf, strlen(buf));
						if(w < 0)
							perror("socket write");
					}
					else{	//send to client
						printf("UMSG from %s to %s\n",sender->name, cli->name);
						int len = strlen(buf);
						buf[len] = '\n';
						buf[len+1] = '\0';
						w = write(cli->fd, buf, strlen(buf));
						if(w < 0)
							perror("socket write");
					}
				}
			}
			else if(strcmp(tempbuf,"BMSG")==0){
				Client cli = findClientbyfd(head, self_fd);
				if(cli == NULL) {
					memset(buf, 0, MSG_SIZE);
					strcpy(buf, "You haven't joined a group");
					int len = strlen(buf);
					buf[len] = '\n';
					buf[len + 1] = '\0';
					int w = write(self_fd, buf, strlen(buf));	//write to socket
					if(w < 0)
						perror("socket write");
				}
				else{
					char bmsg[MSG_SIZE], temp2[MSG_SIZE];
					sscanf(buf, "%s %[^\n]", temp2, bmsg);
					memset(buf,0,MSG_SIZE);
					temp = head;
					sprintf(buf,"[%s]->",cli->name);
					strcat(buf, bmsg);
					printf("BMSG from client %s\n", cli->name);
					int len = strlen(buf);
					buf[len] = '\n';
					buf[len+1] = '\0';
					while(temp!=NULL){	
						int w = write(temp->fd, buf, strlen(buf));	//write to socket
						if(w < 0)
							perror("socket write");
						temp=temp->next;
					}
				}
			}
			//READER END

			else {
				printf("Command %s not recognized\n",tempbuf);
				sprintf(buf, "Command %s not recognized\n",tempbuf);
				int w = write(self_fd, buf, strlen(buf));	//write to socket
				if(w < 0)
					perror("socket write");
			}

			
			pthread_mutex_lock(&mutex);
			reading--;
			pthread_mutex_unlock(&mutex);
			pthread_cond_broadcast(&cond);
		}
	}
}

Client addClient(Client head,char *name,int fd,char retmsg[100]){	//makes client as new head
	
	if(head==NULL){
		printf("Client %s joined\n",name);
		strcpy(retmsg,"You are online");
		head = newClient(fd,name);
		return head;
	}
	Client x = findClientbyName(head,name);
	Client y = findClientbyfd(head,fd);
	if(y != NULL){
		printf("This terminal already has a client %s\n",y->name);
		sprintf(retmsg, "This terminal already has a client with name %s", y->name);
		return head;
	}
	else if(x != NULL){
		printf("Client %s already joined\n",name);
		sprintf(retmsg, "There is already a user with name %s", name);
		return head;
	}
	printf("Client %s joined\n",name);
	strcpy(retmsg,"You are online");
	Client cli = newClient(fd,name);
	cli->next=head;
	return cli;
}

Client newClient(int fd,char *name){	//creates new Client
	Client cli = (Client)malloc(sizeof(struct client));
	cli->fd=fd;
	strcpy(cli->name,name);
	cli->next=NULL;
	return cli;
}

Client findClientbyName(Client head,char *name){
	Client temp = head;
	
	while(temp!=NULL && strcmp(temp->name,name)!=0)
		temp=temp->next;

	return temp;	//if client found,returns client,else NULL
}

Client findClientbyfd(Client head,int fd){
	Client temp = head;
	
	while(temp!=NULL && temp->fd!=fd)
		temp=temp->next;

	return temp;	//if client found,returns client,else NULL
}

Client deleteClientbyfd(Client head,int fd,char retmsg[100]){
	if(head==NULL)
		return head;
	Client prev,temp=head;
	if(head->fd==fd){	//if fd in first node
		temp=head->next;
		printf("Client %s left group \n",head->name);
		strcpy(retmsg,"You left the group\n");
		free(head);
		return temp;
	}
	temp=head->next;prev=head;
	while(temp!=NULL ){
		if(temp->fd==fd){	//if in middle node
			prev->next=temp->next;
			printf("Client %s left group \n",temp->name);
			strcpy(retmsg,"You left the group");
			free(temp);
			return head;
		}
		prev=temp;
		temp=temp->next;
	}
	return head;
}
