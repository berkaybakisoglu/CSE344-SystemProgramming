#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <time.h>
 #include <pthread.h>
#define MAXSTUDENTS 200
#define MAXHOMEWORKS 5000


char *studentsFilePath,*homeworkFilePath;
char homeworkBuffer[MAXHOMEWORKS];
int programEnds=0;
int bufferCount=0;
int doneCount=0;
int moneyMadeByStudents=0;
FILE* studentsFP;
int totalStudentNumber;
int myMoney;
pthread_t* threads;
int allHomeworksRetrieved;

struct studentsForHire{
	char name[50];
	int quality;
	int speed;
	int price;
	int ID;
	int working;
	char myHomeworkType;
	int homeworkSolved;
};
struct studentsForHire studentsArray[MAXSTUDENTS];
struct studentsForHire studentsArrayByQuality[MAXSTUDENTS];
struct studentsForHire studentsArrayBySpeed[MAXSTUDENTS];
struct studentsForHire studentsArrayByPrice[MAXSTUDENTS];
sem_t bufferFull;
sem_t readyWorker;
sem_t dataLock;
sem_t *workerSemaphore;
void printStructEnd(struct studentsForHire sfh);
void printEndMessage();
void printAllArrays();
int findPrice();
int findSpeed();
int findQuality();
void sortStructsByPrice();
void sortStructsBySpeed();
void sortStructsByQuality();
void setStudentReset(int myID);
void setStudentWorking(int myID,char homeworkType);
void* cheaterStudent(void* arg);
void* workerStudent(void* studentData);
void assignStudents();
void signalHandling(int signum) {
	if(signum==SIGINT){
		programEnds=1;
	
	}
}
int main(int argc, char** argv){
	signal(SIGINT,signalHandling);
	signal(SIGTERM,signalHandling);
	if (argc != 4)
	{
		fprintf(stderr,"Please enter command line parameters ! \n");
		fprintf(stdout,"Usage like = $./hw4 homeworkFilePath studentsFilePath 10000");
		exit (0);
	}
	homeworkFilePath=argv[1];
	studentsFilePath=argv[2];
	sscanf(argv[3],"%d",&myMoney);
	if(myMoney<=0){
		printf("Money should more than 0 \n");
		exit(0);
	}
	for(int i=0;i<MAXHOMEWORKS;i++){
		homeworkBuffer[i]=' ';
	}
	assignStudents();
	workerSemaphore=(sem_t*)malloc(sizeof(sem_t)*totalStudentNumber);
	for(int i = 0; i<totalStudentNumber; i++)
    	sem_init(&workerSemaphore[i], 0, 0);
 
	sem_init(&bufferFull,1,0);
	sem_init(&readyWorker,0,0);
	sem_init(&dataLock,0,1);
	allHomeworksRetrieved=0;
	//printf("TOtal studentsArray = %d",totalStudentNumber);
	for(int i=0;i<totalStudentNumber;i++){
		studentsArrayByQuality[i]=studentsArray[i];
		studentsArrayBySpeed[i]=studentsArray[i];
		studentsArrayByPrice[i]=studentsArray[i];
	}

	sortStructsByQuality();
	sortStructsBySpeed();
	sortStructsByPrice();
	threads=(pthread_t*)malloc(sizeof(pthread_t)*totalStudentNumber+sizeof(pthread_t)*2);
	threads[0]=pthread_self();
	if(pthread_create(&threads[1], NULL,cheaterStudent,NULL) != 0 )
		exit(1);

	for(int i=0;i<totalStudentNumber;i++){
		if(pthread_create(&threads[i+2], NULL,workerStudent,&studentsArray[i]) != 0 )
			exit(1);
	}
	
	while(allHomeworksRetrieved==0 || (allHomeworksRetrieved==1 && bufferCount!=0)){
		if(programEnds==1)
			break;
		sem_wait(&bufferFull);
		sem_wait(&readyWorker);
		int selected=-1;
		for(int i=0;i<MAXHOMEWORKS;i++){
			if(homeworkBuffer[i]!=' '){
				if(homeworkBuffer[i]=='Q')
					selected=findQuality();
				else if(homeworkBuffer[i]=='S')
					selected=findSpeed();
				else if(homeworkBuffer[i]=='C')
					selected=findPrice();
				if(selected==-1 && myMoney<studentsArrayByPrice[0].price){
					printf("Money has finished\n");
					programEnds=1;
					break;
				}
				else if(selected==-1)
					sem_post(&bufferFull);
				if(selected!=-1){
					setStudentWorking(selected,homeworkBuffer[i]);
					doneCount++;			
					bufferCount--;
					homeworkBuffer[i]=' ';
				}
				break;
			}
		}
	}
	programEnds=1;
	for(int j=0;j<totalStudentNumber;j++){
		sem_post(&workerSemaphore[j]);
	}
	void* returnValue;
	for(int i=0;i<totalStudentNumber+1;i++){
		pthread_join(threads[i+1],&returnValue);
	}
	printEndMessage();
	free(threads);
	sem_destroy(&bufferFull);
	sem_destroy(&dataLock);
	for(int i=0;i<totalStudentNumber;i++){
		sem_destroy(&workerSemaphore[i]);
	}
	free(workerSemaphore);
	

}
void assignStudents(){
    char *line = NULL;
    size_t len = 0;
    char* temp,*token;
    int totalLines=0;
    int tokenNo=0;
    ssize_t nread;
	studentsFP=fopen(studentsFilePath,"r");
	if(studentsFP==NULL){
		printf("Read file error !");
		exit(0);
	}
	while ((nread = getline(&line, &len,studentsFP) != -1)) {
		temp=line;
		token = strtok(temp," ");
		strcpy(studentsArray[totalLines].name,token);
		while(token != NULL){
			if(tokenNo==1)
				studentsArray[totalLines].quality=atoi(token);
			else if(tokenNo==2)
				studentsArray[totalLines].speed=atoi(token);
			else if(tokenNo==3)
				studentsArray[totalLines].price=atoi(token);
			token = strtok(NULL," ");
			tokenNo++;
	}
	studentsArray[totalLines].ID=totalLines;
	studentsArray[totalLines].myHomeworkType=' ';
	studentsArray[totalLines].working=0;
	studentsArray[totalLines].homeworkSolved=0;
	tokenNo=0;
	totalLines++;
	

}
totalStudentNumber=totalLines-1;
printf("%d students-for-hire threads have been created.\n",totalStudentNumber);
printf("Name Q S C \n");
printAllArrays();
fclose(studentsFP);
free(temp);
}
void printStruct(struct studentsForHire sfh){
	printf("%s ",sfh.name);
	printf("%d ",sfh.quality);
	printf("%d ",sfh.speed);
	printf("%d ",sfh.price);
	
}
void* workerStudent(void* studentData){
	struct studentsForHire* myData=(struct studentsForHire*) studentData;
	while(1){
		setStudentReset(myData->ID);
		sem_post(&readyWorker);
		printf("%s is waiting for a homework\n",myData->name );
		sem_wait(&workerSemaphore[myData->ID]);
		if(programEnds==1){
			sem_post(&bufferFull);
			sem_post(&readyWorker);
			pthread_exit(NULL);
		}
		myData->homeworkSolved+=1;
		moneyMadeByStudents+=myData->price;
		printf("%s is solving homework %c for %d ,G has %dTL left \n",myData->name,myData->myHomeworkType,myData->price,myMoney );
		sleep(6-myData->speed);
		
	}
	free(myData);

}

void* cheaterStudent(void* arg){
	int fd;
	char temp;
	fd=open(homeworkFilePath,O_RDONLY);
	if(fd<0){
		printf("Error on reading file,EXIT");
		raise(SIGINT);
}
		while(read(fd,&temp,sizeof(char))){
			if(programEnds==1){
				sem_post(&bufferFull);
				sem_post(&readyWorker);
				close(fd);
				pthread_exit(0);
	}
			if(myMoney<studentsArrayByPrice[0].price){
				printf("H has no more money for homeworks, terminating.\n");
				close(fd);
				exit(0);
			}
			if(temp=='C' || temp=='S' || temp=='Q'){
				for(int i=0;i<MAXHOMEWORKS;i++){
					if(homeworkBuffer[i]==' '){
						homeworkBuffer[i]=temp;
						bufferCount++;
						printf("H has a new homework %c,remaining money = %d\n",temp,myMoney);
						sem_post(&bufferFull);
						break;
					}
				}
			}
		}
		printf("H has no other homeworks, terminating. \n");
		close(fd);
		allHomeworksRetrieved=1;
		pthread_exit(NULL);
}
void setStudentWorking(int myID,char homeworkType){
	sem_wait(&dataLock);
	for(int i=0;i<totalStudentNumber;i++){
		if(studentsArrayBySpeed[i].ID==myID){
			studentsArrayBySpeed[i].working=1;
			studentsArrayBySpeed[i].myHomeworkType=homeworkType;
			
		}
	}
	for(int i=0;i<totalStudentNumber;i++){
		if(studentsArrayByQuality[i].ID==myID){
			studentsArrayByQuality[i].working=1;
			studentsArrayByQuality[i].myHomeworkType=homeworkType;
			
		}
	}
	for(int i=0;i<totalStudentNumber;i++){
		if(studentsArrayByPrice[i].ID==myID){
			studentsArrayByPrice[i].working=1;
			studentsArrayByPrice[i].myHomeworkType=homeworkType;
			
		}
	}
	for(int i=0;i<totalStudentNumber;i++){
		if(studentsArrayByPrice[i].ID==myID){
			studentsArrayByPrice[i].working=1;
			studentsArrayByPrice[i].myHomeworkType=homeworkType;
			
		}
	}
	studentsArray[myID].working=1;
	studentsArray[myID].myHomeworkType=homeworkType;
	myMoney-=studentsArray[myID].price;
	sem_post(&dataLock);
	sem_post(&workerSemaphore[myID]);
}
void setStudentReset(int myID){
	sem_wait(&dataLock);
	for(int i=0;i<totalStudentNumber;i++){
		if(studentsArrayBySpeed[i].ID==myID){
			studentsArrayBySpeed[i].working=0;
			studentsArrayBySpeed[i].myHomeworkType=' ';
			
		}
	}
	for(int i=0;i<totalStudentNumber;i++){
		if(studentsArrayByQuality[i].ID==myID){
			studentsArrayByQuality[i].working=0;
			studentsArrayByQuality[i].myHomeworkType=' ';
			
		}
	}
	for(int i=0;i<totalStudentNumber;i++){
		if(studentsArrayByPrice[i].ID==myID){
			studentsArrayByPrice[i].working=0;
			studentsArrayByPrice[i].myHomeworkType=' ';
			
		}
	}
	for(int i=0;i<totalStudentNumber;i++){
		if(studentsArrayByPrice[i].ID==myID){
			studentsArrayByPrice[i].working=0;
			studentsArrayByPrice[i].myHomeworkType=' ';
			
		}
	}
	studentsArray[myID].working=0;
	studentsArray[myID].myHomeworkType=' ';
	sem_post(&dataLock);
}
void sortStructsByQuality(){
	struct studentsForHire temp;
	for(int i=0;i<totalStudentNumber-1;i++){
		for(int j=0;j<totalStudentNumber-1;j++){
			if(studentsArrayByQuality[j].quality<studentsArrayByQuality[j+1].quality){
				temp=studentsArrayByQuality[j+1];
				studentsArrayByQuality[j+1]=studentsArrayByQuality[j];
				studentsArrayByQuality[j]=temp;
			}
		}
	}
}
void sortStructsBySpeed(){
	struct studentsForHire temp;
	for(int i=0;i<totalStudentNumber-1;i++){
		for(int j=0;j<totalStudentNumber-1;j++){
			if(studentsArrayBySpeed[j].speed<studentsArrayBySpeed[j+1].speed){
				temp=studentsArrayBySpeed[j+1];
				studentsArrayBySpeed[j+1]=studentsArrayBySpeed[j];
				studentsArrayBySpeed[j]=temp;
			}
		}
	}
}
void sortStructsByPrice(){
	struct studentsForHire temp;
	for(int i=0;i<totalStudentNumber-1;i++){
		for(int j=0;j<totalStudentNumber-1;j++){
			if(studentsArrayByPrice[j].price>studentsArrayByPrice[j+1].price){
				temp=studentsArrayByPrice[j+1];
				studentsArrayByPrice[j+1]=studentsArrayByPrice[j];
				studentsArrayByPrice[j]=temp;
			}
		}
	}
}
int findQuality(){
	int selected=-1;
	for(int i=0;i<totalStudentNumber;i++){
		if(studentsArrayByQuality[i].working==0 && myMoney-studentsArrayByQuality[i].price>=0){
			selected=studentsArrayByQuality[i].ID;
			break;
		}
	}
	return selected;
}
int findSpeed(){
	int selected=-1;
	for(int i=0;i<totalStudentNumber;i++){
		if(studentsArrayBySpeed[i].working==0 && myMoney-studentsArrayBySpeed[i].price>=0){
			selected= studentsArrayBySpeed[i].ID;
			break;
		}
	}

	return selected;
}
int findPrice(){
	int selected=-1;
	for(int i=0;i<totalStudentNumber;i++){
		if(studentsArrayByPrice[i].working==0 && myMoney-studentsArrayByPrice[i].price>=0){
			selected=studentsArrayByPrice[i].ID;
			break;
		}
	}
	return selected;
}
void printAllArrays(){
		for(int i=0;i<totalStudentNumber;i++){
			printStruct(studentsArray[i]);
			printf("\n");
		}
}
void printEndMessage(){
	printf("Homeworks solved and money made by the students: \n");
		for(int i=0;i<totalStudentNumber;i++){
			printStructEnd(studentsArray[i]);
			printf("\n");
		}
		printf("Total cost for %d homework is = %d TL\n",doneCount,moneyMadeByStudents);
		printf("Money left in G's account = %d TL\n",myMoney);
}
void printStructEnd(struct studentsForHire sfh){
	printf("%s ",sfh.name);
	printf("%d ",sfh.homeworkSolved);
	printf("%d ",sfh.homeworkSolved*sfh.price);
	
}
