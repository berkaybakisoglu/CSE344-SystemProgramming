#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <time.h>
#include <math.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>
#include <signal.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <pthread.h> 
#include <time.h>
#include <math.h>
#include <sys/socket.h> 
#include <arpa/inet.h> 
#include <sys/resource.h>  
#include <sys/stat.h>
#include <limits.h>
#include <errno.h>
#include "SocketQueue.h"
#include <semaphore.h>
#include <sys/mman.h>
#define MAXSTRINGSIZE 200
#define MAXQUERYCOLUMNS 50
char * allTable();
#define SERVEREXISTFILE "serverexist"
void readData();
void daemonizeServer();
int isServerExist();
void addTimeStamp();
void* workerThread();
char *strtok_single (char * str, char const * delims);
char* parseRequest();
char * selectColumns(char* requests);
int* getValidLabels(char columns[][MAXSTRINGSIZE],int size);
char * selectUpdateColumns(char* requests);
char* getDistinctColumns(char columns[][MAXSTRINGSIZE],int size);
void freeDataset();
char * selectDistinctColumns(char* requests);
void recvAll(int fd,char* receivedMessage , size_t requestSize);
void sendAll(int fd,char* receivedMessage , size_t requestSize);
void countOfLabels();
void countOfLines();
int countRecords(char* respond);
char ***dataArray;


pthread_t* threads;

FILE* Log;
int serverSock=0;
char logFile[PATH_MAX];
char datasetPath[PATH_MAX];
int PORT;
FILE* inputFile;
volatile int isExiting=0;
int labelCounter=0,lineCounter=0,dataLines=0;
int startingThreads=0;
int activeThreads=0;
int readyThreads=0;
int workingThreads=0;
int readingThreads=0;
int waitingReading=0;
int waitingWriting=0;
int writingThreads=0;
int currentSock;
pthread_cond_t mainWaitWorkers=PTHREAD_COND_INITIALIZER;
pthread_cond_t workerWaitsJob=PTHREAD_COND_INITIALIZER;
pthread_cond_t validReader=PTHREAD_COND_INITIALIZER;
pthread_cond_t validWriter=PTHREAD_COND_INITIALIZER;
pthread_mutex_t readyThreadMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t workerMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t workingThread=PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t readerMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t writerMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t fileMutex = PTHREAD_MUTEX_INITIALIZER;

void signalHandling(int signum)
{
	switch(signum) {
		case SIGINT:
		remove(SERVEREXISTFILE);
		addTimeStamp();
		if(threads==NULL)
			exit(0);
		if(Log!=NULL)
			fprintf(Log,"Termination signal received, waiting for ongoing threads to complete.\n");
		else{
			printf("Termination signal received, waiting for ongoing threads to complete.\n");
		}
		isExiting=1;
		pthread_cond_broadcast(&workerWaitsJob);
/*		if(threads!=NULL){
		for(int i =0;i<startingThreads;i++){
			kill(SIGTERM,threads[i]);
		}
		
}
*/		close(serverSock);
		pthread_cond_broadcast(&mainWaitWorkers);

		
	}

}


void log_message(char* message){
	addTimeStamp();
	pthread_mutex_lock(&fileMutex);
	fprintf(Log,"%s",message);
	pthread_mutex_unlock(&fileMutex);
	fflush(Log);
}
void* workerThread(void* i){
	
	int myJob=-1; // 0 reader 1 writer
	int fd;	
	int requestSize=0;
	int myID = *((int*) i);

	
	//sleep(40);
	while(isExiting==0){
		pthread_mutex_lock(&readyThreadMutex);
		readyThreads++;
		pthread_mutex_lock(&fileMutex);
		addTimeStamp();
		fprintf(Log, "Thread #%d: waiting for connection\n",myID );
		fflush(Log);
		pthread_mutex_unlock(&fileMutex);
		printf("Thread #%d: waiting for connection\n",myID );
		pthread_cond_signal(&mainWaitWorkers);
		pthread_mutex_unlock(&readyThreadMutex);

		pthread_mutex_lock(&workerMutex);
		while(isSocketQueueEmpty()){
			if(isExiting==1)
				pthread_exit(NULL);
			pthread_cond_wait(&workerWaitsJob, &workerMutex);
		}

		fd=socketDequeue();
		if(fd<0){
			pthread_exit(0);
		}
		printf("A connection has been delegated to thread id #%d.\n",myID);
		pthread_mutex_lock(&fileMutex);
		addTimeStamp();
		fprintf(Log,"A connection has been delegated to thread id #%d.\n",myID);
		fflush(Log);
		pthread_mutex_unlock(&fileMutex);
		pthread_mutex_unlock(&workerMutex);
		pthread_mutex_lock(&readyThreadMutex);
		readyThreads--;
		pthread_mutex_unlock(&readyThreadMutex);
		while(1){
			if(recv(fd ,&requestSize,sizeof(int),0)<0){
				addTimeStamp();
				perror("Package cant be get,exiting.\n");
				exit(0);
			}
			if(requestSize==-999){
				close(fd);
				break;
			}
			else if(requestSize==-1){
				addTimeStamp();
				perror("Package cant be get,exiting.\n");
				exit(0);
			}
			//printf("Size of request = %d\n",requestSize);

			char* receivedMessage=malloc(sizeof(char)*requestSize);

			recvAll(fd,receivedMessage,requestSize);
			pthread_mutex_lock(&fileMutex);
			addTimeStamp();
			fprintf(Log,"Thread #%d: received query ‘%s‘.\n",myID,receivedMessage);
			fflush(Log);
			pthread_mutex_unlock(&fileMutex);
			//printf("\n%d %s \n",myID,receivedMessage);
			char *ret;
			ret = strstr(receivedMessage, "SELECT");
			if (ret) {
    			//printf("Im reader \n");
				myJob=0;
				waitingReading++;
			}
			else {
				ret = strstr(receivedMessage, "UPDATE");
				if (ret){
    				//printf("Im writer \n");
					myJob=1;
					waitingWriting++;
				}
				else{

					myJob=-1;

				}
			}
			if(myJob==0){
				pthread_mutex_lock(&readerMutex);
				while(waitingWriting+writingThreads!=0){
			//		printf("Im waiting writers to finish as reader \n");
					pthread_cond_wait(&validReader, &readerMutex);
				}
				readingThreads++;
				waitingReading--;
			//	printf("Ok Im reader and i can read \n");
				pthread_mutex_unlock(&readerMutex);
				

			}
			else if(myJob==1){
				pthread_mutex_lock(&writerMutex);
				while(writingThreads!=0){
			//		printf("Im waiting writers to finish as another writer \n");
					pthread_cond_wait(&validWriter, &writerMutex);
				}
				writingThreads++;
				waitingWriting--;
			//	printf("Ok Im writer and i can write \n");
				pthread_mutex_unlock(&writerMutex);


			}
			//printf("Im sending the respond\n");
			char* respond=parseRequest(receivedMessage);
			int respondSize=strlen(respond);
			usleep(500000);
			if(send(fd,&respondSize,sizeof(int),0)<0){
				addTimeStamp();
				log_message("Package cant be send,exiting.\n");
				fflush(Log);
				exit(0);
			}
			sendAll(fd,respond,respondSize);
			
			if(myJob==0){
				pthread_mutex_lock(&readerMutex);
				readingThreads--;
				pthread_cond_signal(&validReader);
				pthread_cond_signal(&validWriter);
				pthread_mutex_unlock(&readerMutex);
			}
			else if(myJob==1){
				pthread_mutex_lock(&writerMutex);
				writingThreads--;
				pthread_cond_signal(&validReader);
				pthread_cond_signal(&validWriter);

				pthread_mutex_unlock(&writerMutex);
			}
			myJob=-1;
			pthread_mutex_lock(&fileMutex);
			addTimeStamp();
			fprintf(Log,"Thread #%d: query completed %d records have been returned.\n",myID,countRecords(respond));
			pthread_mutex_unlock(&fileMutex);
			fflush(Log);
			free(receivedMessage);
		}
	}
	return NULL;
}



int main(int argc, char** argv){
	int opt=0;
	int pFlag=0;
	int oFlag=0;
	int lFlag=0;
	int dFlag=0;

	if (argc != 9)
	{
		fprintf(stderr,"Please enter command line parameters ! \n");
		fprintf(stdout,"./server -p PORT -o pathToLogFile -l poolSize -d datasetPath\n ");
		exit (0);
	}

	while ((opt = getopt(argc, argv, "p:o:l:d:")) != -1) {  
		switch (opt){
			case 'p':
			pFlag=1;
			PORT = atoi(optarg);
			if(PORT<0){
				fprintf(stdout,"PORT MUST BE A POSITIVE INTEGER");
				exit(0);
			}
			break;
			case 'o':
			oFlag=1;
			strcpy(logFile,optarg);
			if(logFile==NULL){		
				fprintf(stdout,"LOGFILE ERROR");
				exit(0);}

				break;
				case 'l':
				lFlag=1;
				startingThreads=atoi(optarg);
				if(startingThreads<2){
					fprintf(stdout,"Server cannot start with less than 2 threads");
				}
				break;
				case 'd':
				dFlag=1;
				strcpy(datasetPath,optarg);
				if(datasetPath==NULL){		
					fprintf(stdout,"DATASET ERROR");
					exit(0);}
					break;

					default:
					fprintf(stdout,"Usage like = $./server -p PORT -o pathToLogFile -l poolSize -d datasetPath\n");
					exit(0);
				}
			}

			if(pFlag!=1 || oFlag!=1 ||  lFlag!=1 || dFlag!=1){

				fprintf(stdout,"Usage like = $./server -p PORT -o pathToLogFile -l poolSize -d datasetPath\n");
				exit(0);
			}
			daemonizeServer();
	//readData();
		}

		void readData(){
			FILE* inputFile=fopen(datasetPath,"r");
			if(inputFile==NULL){
				addTimeStamp();
				fprintf(Log,"File cannot found,exitting\n");
				unlink(SERVEREXISTFILE);
				exit(0);
			}
			fclose(inputFile);
			countOfLabels();
			countOfLines();
			struct timeval start, end;
			gettimeofday(&start,NULL);
			inputFile=fopen(datasetPath,"r");
			if(inputFile==NULL){
				addTimeStamp();
				fprintf(Log,"File cannot found,exitting\n");
				unlink(SERVEREXISTFILE);
				exit(0);
			}
			char* line;
			size_t linelen=0;
			ssize_t nread=0;
			char* token;

			char linecopy[2000];

			dataArray = (char***)malloc(sizeof(char**) * labelCounter);
			for(int i = 0; i <labelCounter; ++i){
				dataArray[i] = (char**)malloc(sizeof(char*) * lineCounter);
				for(int j = 0; j < lineCounter; ++j){
					dataArray[i][j] = (char*)malloc(sizeof(char) * MAXSTRINGSIZE);        }
				}
				int linec=0;
				int labelc=0;
				while((nread=getline(&line,&linelen,inputFile) != -1)) {
					if(line[strlen(line)-1]=='\n')
						line[strlen(line)-1]='\0';
					strcpy(linecopy,line);

					token = strtok_single(linecopy,",");

					while(token) {
						strcpy(dataArray[labelc++][linec],*token ? token : "NULL");
						token = strtok_single(NULL, ",");
					}
					labelc=0;
					linec++;
				}

				gettimeofday(&end,NULL);
				double timeMeasured=((double)((end.tv_sec * 1000 + end.tv_usec / 1000) - (start.tv_sec * 1000 + start.tv_usec / 1000) )/ (double)1000);
				addTimeStamp();
				fprintf(Log, "Dataset loaded in %.2lf seconds with %d records\n",timeMeasured,dataLines);
				fflush(Log);
				fclose(inputFile);

			}


			char* parseRequest(char* requests){
/*	char requests[][100]={"1 SELECT * FROM TABLE;", 
"1 SELECT columnName1, columnName2, columnName3 FROM TABLE;",
"2 UPDATE TABLE SET columnName1=’value1’, columnName2=’value2’ WHERE columnName=’valueX’",
"1 SELECT DISTINCT columnName1,columnName2 FROM TABLE;"
"SELECT DISTINCT * FROM TABLE"
};*/
				char select[8];
				char update[18];
				char distinct[18];
				char* token;
				token = strstr(requests," ");
				requests=token+1;

				strncpy(distinct, requests, 15);
				update[15] = 0;
				if(strcmp("SELECT DISTINCT",distinct)==0){
					requests=&requests[16];

					return selectDistinctColumns(requests);
				}
				printf("%s",requests);
				char* ret=strstr(requests, "SELECT * FROM TABLE;");
				if (ret){
					return allTable();
				}

				strncpy(select, requests, 6);
				select[7] = 0;


				if(strcmp(select,"SELECT")==0){
					requests=&requests[7];
					return selectColumns(requests);

				}
				strncpy(update, requests, 16);
				update[16] = 0;
				if(strcmp("UPDATE TABLE SET",update)==0){
					requests=&requests[17];
					return selectUpdateColumns(requests);
				}


				return "SYNTAX ERROR";
			}
			char * allTable(){
				char * respond=malloc(sizeof(char*)*lineCounter*labelCounter*MAXSTRINGSIZE+lineCounter*2);
				respond[0]='\0';
				for(int i = 0; i < lineCounter ; i++){
					for(int j=0;j< labelCounter; j++){
						strcat(respond,dataArray[j][i]);
						strcat(respond," ");

					}
					strcat(respond,"\n");
					

				}

	
				return respond;

			}
			char* getColumns(char columns[][MAXSTRINGSIZE],int size){
				char * respond=malloc(sizeof(char*)*lineCounter*labelCounter*MAXSTRINGSIZE+lineCounter*2);
				respond[0]='\n';


				int * selected=getValidLabels(columns,size);
				for(int i =0;i<size;i++){
					if(selected[i]==-1)
						return "syntaxError";
				}

				for(int i = 0; i < lineCounter ; i++){
					for(int j=0;j< labelCounter; j++){
						int checkValue=0;
						for(int k = 0 ; k<size;k++){
							if(selected[k]==j)
								checkValue=1;
						}
						if(checkValue==1){
							strcat(respond,dataArray[j][i]);
							strcat(respond," ");
						}

					}
					strcat(respond,"\n");
				}
				free(selected);

				return respond;

			}
			char* updateColumns(char columns[][MAXSTRINGSIZE],int size){

				char updatelabels[size][MAXSTRINGSIZE];
				char values[size][MAXSTRINGSIZE];
				char* token;
				char* respond = malloc(sizeof(char)*100);
				int sw=0;

				for(int i = 0 ;i<size;i++){
					token = strtok(columns[i],"=");
					while(token) {
						if(sw==0){
							strcpy(updatelabels[i],token);
							sw++;
						}
						else if(sw==1){
							strcpy(values[i],token);
							sw--;}
							token = strtok(NULL,"=");
						}

					}
					int * selected=getValidLabels(columns,size);
					int newValIndex=0;
					int lineChange=0;
					int totalDataChanged=0;

					for(int i = 0; i < lineCounter ; i++){

						if(strcmp(dataArray[selected[size-1]][i],values[size-1])==0){
							lineChange=1;
						}
		//printf("%d \t %s \n",lineChange,dataArray[selected[size-1]][i]);
						for(int j=0;j< labelCounter; j++){
							int checkValue=0;
							for(int k = 0 ; k<size-1;k++){
								if(selected[k]==j){

									newValIndex=k;
									checkValue=1;
					//printf("values = (%s)\n",values[k]);
								}
							}
							if(checkValue==1 && lineChange==1){
								totalDataChanged++;
								strcpy(dataArray[j][i],values[newValIndex]);

							}

						}
						lineChange=0;

					}
					free(selected);
					sprintf(respond,"Total number of changed datas = %d\n",totalDataChanged);

					return respond;
				}
				int* getValidLabels(char columns[][MAXSTRINGSIZE],int size){
					int* selected=malloc(sizeof(int)*size);
					int selectIndex=0;
					for(int i = 0 ; i < size ; i++){
						selected[i]=-1;
					}


					for(int i = 0 ; i < size ; i++){
						for(int j = 0 ; j<labelCounter;j++){
			//printf("%d %d ",strlen(columns[i]),strlen(dataArray[j][0]));
			//printf("%s1%s1\n",columns[i],dataArray[j][0]);

							if(strcmp(columns[i],dataArray[j][0])==0){

								selected[selectIndex++]=j;
							}
						}

					}

					return selected;

				}
				char * selectColumns(char* requests){
					char withoutWhiteSpace[MAXQUERYCOLUMNS*MAXSTRINGSIZE];
					char columnNames[MAXQUERYCOLUMNS][MAXSTRINGSIZE];
					int columnNameIndex=0;
					int newIndex=0;
					char* token;
	//printf("%s",requests);
					char* index = strstr(requests, " FROM TABLE;");
					if(index==NULL){
						return "Syntax Error";
					}
					int position = index-requests;
					for(int i = 0 ; i<position ; i++){
						if(requests[i]!=' ' && requests[i]!='\'')
							withoutWhiteSpace[newIndex++]=requests[i];
					}
					withoutWhiteSpace[newIndex]='\0';
	//printf("%s",withoutWhiteSpace);
					token = strtok(withoutWhiteSpace,",");
   		//sleep(1);
					while(token) {
						strcpy(columnNames[columnNameIndex++],token);
						token = strtok(NULL, ",");
					}
					return getColumns(columnNames,columnNameIndex);

				}
				char * selectUpdateColumns(char* requests){
					char withoutWhiteSpace[MAXQUERYCOLUMNS*MAXSTRINGSIZE];
					char columnNames[MAXQUERYCOLUMNS][MAXSTRINGSIZE];
					int columnNameIndex=0;
					int newIndex=0;
					char* token;

					char* index = strstr(requests, " WHERE");
					if(index==NULL){
						return "Syntax Error";
					}
					int position = index-requests;
					for(int i = 0 ; i<position ; i++){
						if(requests[i]!=' ' && requests[i]!='\'')
							withoutWhiteSpace[newIndex++]=requests[i];
					}
					withoutWhiteSpace[newIndex++]=',';
					for(int i = position+7 ; i<strlen(requests) ; i++){
						if(requests[i]!=' ' && requests[i]!='\'')
							withoutWhiteSpace[newIndex++]=requests[i];
					}
					withoutWhiteSpace[newIndex]='\0';


					token = strtok(withoutWhiteSpace,",");
   		//sleep(1);
					while(token) {
						strcpy(columnNames[columnNameIndex++],token);
						token = strtok(NULL, ",");
					}

					return updateColumns(columnNames,columnNameIndex);

				}
				char* getDistinctColumns(char columns[][MAXSTRINGSIZE],int size){
					char * respond=malloc(sizeof(char*)*lineCounter*labelCounter*MAXSTRINGSIZE+lineCounter*2);
					respond[0]='\0';
					char combinations[lineCounter+1][MAXSTRINGSIZE*size];
					int willNotReturn[lineCounter];
					int wnr=0;
					int * selected=getValidLabels(columns,size);
					for(int i =0;i<size;i++){
						if(selected[i]==-1)
							return "Syntax Error";
					}
					int c=0;

					for(int i = 0; i < lineCounter ; i++){
						for(int j=0;j< labelCounter; j++){
							int checkValue=0;
							for(int k = 0 ; k<size;k++){
								if(selected[k]==j)
									checkValue=1;
							}
							if(checkValue==1){
								strcat(combinations[c],dataArray[j][i]);
								strcat(combinations[c]," ");
							}

						}
						c++;
					}
					for(int i = 0 ; i<c ; i++){
						combinations[i][strlen(combinations[i])]='\n';
					}
					for(int i = 1 ;  i<c-1 ; i++){
						for(int j=i+1;j<c;j++){
							char* check=strstr(combinations[j],combinations[i]);
							if(check){
				//printf("\n i = %d combinations[i] = %s j = %d combinations[i] = %s\n ",i,combinations[i],j,combinations[j]);
								willNotReturn[wnr++]=j;
							}
						}

					}
					int dontAdd=0;
					for(int i = 0 ; i < c;i++){
						for(int j=0 ; j<wnr ; j++ ){
							if(i==willNotReturn[j])
								dontAdd=1;
						}
						if(dontAdd==0){
							strcat(respond,combinations[i]);
						}
						dontAdd=0;
					}

					free(selected);
					return respond;
				}
				char * selectDistinctColumns(char* requests){
					char withoutWhiteSpace[MAXQUERYCOLUMNS*MAXSTRINGSIZE];
					char columnNames[MAXQUERYCOLUMNS][MAXSTRINGSIZE];
					int columnNameIndex=0;
					int newIndex=0;
					char* token;
	//printf("%s",requests);
					char* index = strstr(requests, " FROM TABLE;");
					if(index==NULL){
						return "Syntax Error";
					}
					int position = index-requests;
					for(int i = 0 ; i<position ; i++){
						if(requests[i]!=' ' && requests[i]!='\'')
							withoutWhiteSpace[newIndex++]=requests[i];
					}
					withoutWhiteSpace[newIndex]='\0';
	//printf("%s",withoutWhiteSpace);
					token = strtok(withoutWhiteSpace,",");
   		//sleep(1);
					while(token) {
						strcpy(columnNames[columnNameIndex++],token);
						token = strtok(NULL, ",");
					}
					return getDistinctColumns(columnNames,columnNameIndex);
				}
				void countOfLabels(){
					FILE* inputFile=fopen(datasetPath,"r");
					char* line;
					size_t linelen=0;
					ssize_t nread=0;
					char* token;
					char linecopy[2000];
					if ((nread=getline(&line,&linelen,inputFile) != -1)) {
						strcpy(linecopy,line);
						token = strtok(linecopy,",");
						while( token != NULL ) {
							labelCounter++;
							token = strtok(NULL, ",");
						}
						free(line);
					}
					else{
						log_message("Error on getting labels!");
						remove(SERVEREXISTFILE);
						exit(0);
					}
					if(labelCounter==0){
						printf("Error on getting labels!");
						remove(SERVEREXISTFILE);
						exit(0);
					}
					fclose(inputFile);
				}
				void countOfLines(){
					FILE* inputFile=fopen(datasetPath,"r");
					char ch;
					while(!feof(inputFile))
					{
						ch = fgetc(inputFile);
						if(ch == '\n')
						{
							lineCounter++;
						}
					}
					fclose(inputFile);
					dataLines=lineCounter-1;
				}
				int isServerExist(){

					int fd;
					if((fd=open(SERVEREXISTFILE,O_RDONLY))<0){
						return 1;
					}
					close(fd);
					printf("A SERVER ALREADY OPEN EXIT""\n");
					exit(1);

				}


				void daemonizeServer()
				{
					signal(SIGINT,signalHandling);
					int clientSock;
					struct sockaddr_in server,client;
					int fd;
					int pid,newPid;
					char pidStr[10];
					int n;
					isServerExist();
					Log=fopen(logFile,"a+");
					if (Log<0){
						addTimeStamp();
						printf("LOGFILE CANNOT CREATED EXITTING\n");
						exit(1);
					}
					fd=open(SERVEREXISTFILE,O_RDWR | O_CREAT,0640);
					if (fd<0){
						log_message("SERVEREXISTFILE CANNOT CREATED\n");
						exit(1);
					}
					addTimeStamp();
					fprintf(Log,"Executing with parameters:\n");
					addTimeStamp();
					fprintf(Log,"-p %d\n",PORT);
					addTimeStamp();
					fprintf(Log,"-o %s\n", logFile);
					addTimeStamp();
					fprintf(Log,"-l %d\n", startingThreads);
					addTimeStamp();
					fprintf(Log,"-d %s\n",datasetPath);
					addTimeStamp();
					fprintf(Log, "Loading datasetPath...\n" );
					fflush(Log);
					readData();

					pid=fork();
					if(pid<0){
						perror("ERROR ON FORK");
						exit(1);
					}
					if(pid>0){
						fclose(Log);
						exit(0);
					}
					newPid=setsid();
					if(newPid<0){
						perror("ERROR ON SETSID");
						exit(0);
					}
					umask(022);
					close(STDIN_FILENO);
					close(STDOUT_FILENO);
					close(STDERR_FILENO);
					open("/dev/null",O_RDONLY); 
					open("/dev/null",O_RDWR);
					open("/dev/null",O_RDWR);


					sprintf(pidStr,"%d\n",getpid());
					write(fd,pidStr,strlen(pidStr));
					close(fd);
					


					if ((serverSock = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
					{
						log_message("Socket creation error \n");
						fclose(Log);
						close(serverSock);
						unlink(SERVEREXISTFILE); 
						exit(0); 
					}

					server.sin_family = AF_INET;
					server.sin_addr.s_addr = INADDR_ANY;
					server.sin_port = htons(PORT);
					if(bind(serverSock,(struct sockaddr *)&server , sizeof(server)) < 0)
					{
						log_message("Binding error\n");
						fclose(Log);
						close(serverSock);
						unlink(SERVEREXISTFILE); 
						exit(0); 
					}
					if((listen(serverSock,100))!=0){
						log_message("Error on listening\n");
						close(serverSock);
						fclose(Log);
						unlink(SERVEREXISTFILE);
						exit(1);
					}




					threads=(pthread_t*)malloc(sizeof(pthread_t)*startingThreads);
					int* IDS=(int*)malloc(sizeof(int)*startingThreads);
					for(int i=0;i<startingThreads;i++){
						IDS[i]=i;
						pthread_create(&threads[i], NULL, workerThread, (void *)&IDS[i]);
						activeThreads++;
					}

					addTimeStamp();
					fprintf(Log, "A pool of %d threads has been created\n",activeThreads);
					fflush(Log);

					n=sizeof(struct sockaddr_in);
					while(isExiting==0){
						pthread_mutex_lock(&readyThreadMutex);
						while(readyThreads==0){
							log_message("No thread is available! Waiting…\n");
							fflush(Log);
							printf("No available threads,server is waiting");

							pthread_cond_wait(&mainWaitWorkers,&readyThreadMutex);
						}
						pthread_mutex_unlock(&readyThreadMutex);
						if(isExiting==1)
							break;
						clientSock = accept(serverSock, (struct sockaddr *)&client, (socklen_t*)&n);
						if (clientSock < 0){
							if(isExiting!=1){
								log_message("SOCKET ACCEPT ERROR");
								fclose(Log);
								unlink(SERVEREXISTFILE);
								exit(0);
							}
						}
						socketEnqueue(dup(clientSock));
						close(clientSock);
						pthread_cond_signal(&workerWaitsJob);


					}

					unlink(SERVEREXISTFILE);
					close(serverSock);
					freeDataset();
					addTimeStamp();


					fprintf(Log,"All threads have terminated, server shutting down.");
					fclose(Log);
					exit(0);
				}
				void addTimeStamp(){
					char curTime[100];
					time_t cur = time(NULL);
					strcpy(curTime,ctime(&cur));
					curTime[strlen(curTime)-1]=' ';
					fprintf(Log,"%s",curTime);
					return;
				}
char *strtok_single (char * str, char const * delims) // https://stackoverflow.com/questions/8705844/
{
	static char  * src = NULL;
	char  *  p,  * ret = 0;

	if (str != NULL)
		src = str;

	if (src == NULL)
		return NULL;

	if ((p = strpbrk (src, delims)) != NULL) {
		*p  = 0;
		ret = src;
		src = ++p;

	} else if (*src) {
		ret = src;
		src = NULL;
	}

	return ret;
}
void freeDataset(){
	for(int i = 0; i <labelCounter; ++i){

		for(int j = 0; j < lineCounter; ++j){
			free(dataArray[i][j]);       
		}
		free(dataArray[i]);
	}

}
void recvAll(int fd,char* receivedMessage , size_t requestSize){
	int currentlyPos = 0, readed = 0;
	while(currentlyPos != requestSize){
		readed = recv(fd, receivedMessage + currentlyPos, requestSize - currentlyPos, 0); 
		if(readed < 0){
			fprintf(Log,"Socket error ,exit\n");
			unlink(SERVEREXISTFILE);
			exit(0);
		}
		currentlyPos += readed;   
	}

}
void sendAll(int fd,char* receivedMessage , size_t requestSize){
	int currentlyPos = 0, sent= 0;
	while(currentlyPos != requestSize){
		sent = send(fd, receivedMessage + currentlyPos, requestSize - currentlyPos, 0); 
		if(sent < 0){
			fprintf(Log,"Socket error ,exit\n");
			unlink(SERVEREXISTFILE);
			exit(0);
			break;
		}
		currentlyPos += sent;   
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

