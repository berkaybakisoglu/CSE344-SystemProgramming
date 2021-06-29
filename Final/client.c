#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>
#include <time.h>
#include <math.h>
#include <sys/socket.h> 
#include <arpa/inet.h>
#include <sys/time.h>
#include <linux/limits.h>
#include <errno.h>
void signalHandling(int signum)
{
	switch(signum) {
		case SIGINT:
			exit(0);
			break;
		}

}
#define maxSize 1000
void addTimeStamp();
void recvAll(int fd,char* receivedMessage , size_t requestSize);
int countRecords(char* respond);
int main(int argc, char** argv){
	int opt=0;
	int sock=0;
	int aFlag=0,pFlag=0,iFlag=0,oFlag=0;
	char ID[10];
	char pathToQueryFile[PATH_MAX];
	char line[1500];
	int port;
	char address[100];
	struct sockaddr_in server; 
	double timeMeasured;
	struct timeval start,end;

	if (argc != 9)
	{
		fprintf(stderr,"Please enter command line parameters ! \n");
		fprintf(stdout,"Usage like = $./client -i id -a 127.0.0.1 -p PORT -o pathToQueryFile ");
		exit (0);
	}

	while ((opt = getopt(argc, argv, ":i:a:p:o:")) != -1) {  
		switch (opt){
			case 'a':
			aFlag=1;
			strcpy(address,optarg);
			break;
			case 'p':
			pFlag=1;
			port = atoi(optarg);
			if(port<0){
				fprintf(stderr, "PORT MUST BE POSITIVE\n");
				exit(0);
			}
			break;
			case 'i':
			iFlag=1;
			strcpy(ID,optarg);
			if(atoi(ID)<0){
				perror("ID should be non negative integer \n");
				exit(0);
			}
			break;

			case 'o':
			oFlag=1;
			strcpy(pathToQueryFile,optarg);
			break;

			default:
			fprintf(stdout,"Usage like = $./client -i id -a 127.0.0.1 -p PORT -o pathToQueryFile ");
			exit(0);
		}
	}

	if(aFlag!=1 || pFlag!=1 || iFlag!=1 || oFlag!=1){
		fprintf(stdout,"Usage like = $./client -i id -a 127.0.0.1 -p PORT -o pathToQueryFile ");
		exit(0);
	}
	FILE* inputFile=fopen(pathToQueryFile,"r");
	if(inputFile==NULL){
		fprintf(stdout,"QueryFile cannot opened for reading exiting...");
		exit(0);
	}

	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
	{ 
		addTimeStamp();
		perror(" Socket cant create,exit\n"); 
		exit(0); 
	} 
	server.sin_family = AF_INET; 
	server.sin_port = htons(port);

	if(inet_pton(AF_INET,address, &server.sin_addr)<=0)  
	{ 
		addTimeStamp();
		printf("\nInvalid address/ Address not supported \n"); 
		return -1; 
	} 
	if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0) 
	{ 
		addTimeStamp();
		perror("Connection Failed\n"); 
		return -1; 
	}
	addTimeStamp();

	char *token;
	char linecopy[1500];
	int respondSize=0;
	int endSignal=-999;
	int executedQuery=0;
	while (fgets(line, sizeof(line),inputFile) != NULL) {
		if(strlen(line)>1){

		if(line[strlen(line)-1]=='\n')
			line[strlen(line)-1]='\0';
		strcpy(linecopy,line);
		token = strtok(line," ");
		gettimeofday(&start,NULL);
		int requestSize=strlen(linecopy);
		if(strcmp(token,ID)==0){
			//printf("Client %s connected and sending query size%d\n",ID,requestSize);
			if(send(sock ,&requestSize,sizeof(int),0)<0){
				addTimeStamp();
				perror("Package cant be send,exiting.\n");
				exit(0);
			}
			printf("Client %s connected and sending query %s\n",ID,linecopy);
			if(send(sock ,linecopy,requestSize*sizeof(char),0)<0){
				addTimeStamp();
				perror("Package cant be send,exiting.\n");
				exit(0);
			}
			if(recv(sock ,&respondSize,sizeof(int),0)<0){
				addTimeStamp();
				perror("Package cant be get,exiting.\n");
				exit(0);
			}
			char* respond=malloc(sizeof(char)*respondSize);
			recvAll(sock,respond,respondSize);
			gettimeofday(&end,NULL);
			timeMeasured=((double)((end.tv_sec * 1000 + end.tv_usec / 1000) - (start.tv_sec * 1000 + start.tv_usec / 1000) )/ (double)1000);
			printf("Server’s response to Client-%s is %d records, and arrived in %.2lf seconds\n",ID,countRecords(respond),timeMeasured);
			printf("%s",respond);
			fflush(stdout);
			executedQuery++;
			free(respond);
}
		}
//sleep(5);
	}
	if(send(sock ,&endSignal,sizeof(int),0)<0){
		addTimeStamp();
		perror("Package cant be send,exiting.\n");
		exit(0);
			}
	printf("A total of %d queries were executed, client %s is terminating. \n",executedQuery,ID );
	close(sock);
	




}  
void addTimeStamp(){
	char curTime[100];
	time_t cur = time(NULL);
	strcpy(curTime,ctime(&cur));
	for(int i=0;curTime[i]!='\n';i++){
		printf("%c",curTime[i]);
	}
	printf(" ");
	return;
}
void recvAll(int fd,char* receivedMessage , size_t requestSize){
	int currentlyPos = 0, readed = 0;
	while(currentlyPos != requestSize){
		readed = recv(fd, receivedMessage + currentlyPos, requestSize - currentlyPos, 0); 
		if(readed < 0){
			printf("Socket error ,exit");
			exit(0);
		}
			   currentlyPos += readed;   
	}

}

int countRecords(char* respond){
	int rc=0;
	if(respond==NULL)
		return 0;
	int size=strlen(respond);
	if(size==0){
		return 0;
	}
	for(int i  = 0 ; i<strlen(respond);i++){
		if(respond[i]=='\n')
			rc++;
	}
	return rc;
}

