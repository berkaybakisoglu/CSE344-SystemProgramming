#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <math.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <time.h>

static volatile int aliveChildren=0;
sigset_t interruptMask;             
struct sigaction act;
sem_t* bufferFull;
sem_t* vaccineMutex;
sem_t* vaccineReady;
sem_t* citizenMutex;
sem_t* waitingCitizens;
sem_t* nurseMutex;
int* vaccine1,*vaccine2,*citizensTurn,*citizensLeftTurn,*citizensPIDS,*endedCitizen,*vaccinatorServiced,*nurseCounter;
int vaccinators=0,nurses=0,citizens=0,bufferSize=0,receive2Shots=0;
int input;
int processcount=0;
static volatile sig_atomic_t turnStarted=0,returnSignalReceived=0,allCitizensReady=0;
pid_t* childPids;
pid_t mySignalSender;
int checkReadyVaccines();
void sendBeginSignalToVaccinators();
int getRemainingCitizens();
void* createSharedMemory(size_t size);
void setVaccinatorServices(int myDoses,int myID);
int checkAllCitizens();
int checkCitizensLeft();
pid_t* childPids;
static void signalHandling(int signum,siginfo_t *info,void * context) {
	if(signum==SIGINT){
		if(childPids[0]==getpid()){
			for(int i=1;i<processcount;i++){
				kill(childPids[i+1],SIGTERM);
			}
		}
		else{
			kill(childPids[0],SIGINT);
			free(childPids);
			childPids=NULL;
			exit(0);
		}
	}
	else if(signum==SIGUSR1 && childPids[0]!=getpid()){
		if(!allCitizensReady)
			allCitizensReady++;
		turnStarted=1;
		mySignalSender=info->si_pid;
	}
	else if(signum==SIGUSR2)
		returnSignalReceived=1;
	else if(signum==SIGTERM){
		free(childPids);
		childPids=NULL;
		exit(0);
	}

}

int main(int argc, char** argv){
	int opt=0;
	int nFlag=0,vFlag=0,cFlag=0,bFlag=0,tFlag=0,iFlag=0;
	char* filePath;
	int pid=0,status=0;
	int processcounter=0;
	sigfillset(&interruptMask);
	sigdelset(&interruptMask,SIGINT);
	sigdelset(&interruptMask,SIGUSR1);
	sigdelset(&interruptMask,SIGUSR2);
	sigdelset(&interruptMask,SIGTERM);
	act.sa_sigaction = signalHandling;
	act.sa_mask =  interruptMask;
	act.sa_flags =SA_SIGINFO;
	if (sigaction (SIGINT, &act, NULL) == -1){
		exit(SIGKILL);
	}
	if (sigaction (SIGUSR1, &act, NULL) == -1){
		exit(SIGKILL);
	}
	if (sigaction (SIGUSR2, &act, NULL) == -1){
		exit(SIGKILL);
	}
	if (sigaction (SIGTERM, &act, NULL) == -1){
		exit(SIGKILL);
	}



	if (argc != 13)
	{
		fprintf(stderr,"Please enter command line parameters ! \n");
		fprintf(stdout,"Usage like = $./midterm  -n 3 -v 2 -c 3 -b 11 -t 3 -i inputfilepath ");
		exit (0);
	}
	while ((opt = getopt(argc, argv, "n:v:c:b:t:i:")) != -1) {
		switch (opt){

			case 'n':
			nFlag = 1;
			nurses=atoi(optarg);
			if(nurses<2){
				fprintf(stderr, "Must n >= 2 \n" );
				exit(0);
			}
			break;

			case 'v':
			vFlag = 1;
			vaccinators=atoi(optarg);
			if(vaccinators<2){
				fprintf(stderr, "Must v >= 2 \n" );
				exit(0);
			}
			break;

			case 'c':
			citizens = atoi(optarg);
			cFlag=1;
			if(citizens<3){
				fprintf(stderr, "Must c >= 3 \n" );
				exit(0);
			}
			break;

			case 'b':
			bFlag = 1;
			bufferSize=atoi(optarg);
			break;

			case 't':
			tFlag = 1;
			receive2Shots=atoi(optarg);
			if(receive2Shots<1){
				fprintf(stderr, "Must t >= 1 \n" );
				exit(0);
			}
			break;

			case 'i':
			iFlag=1;
			filePath = optarg;
			break;

			default: 
			fprintf(stdout,"Usage like = $./midterm –n 3 –v 2 –c 3 –b 11 –t 3 –i inputfilepath");
			exit(0);
		}
	}

	if(nFlag!=1 || vFlag!=1 || cFlag!=1 || bFlag!=1 || tFlag!=1 || iFlag!=1){
		fprintf(stdout,"Usage like = $./midterm –n 3 –v 2 –c 3 –b 11 –t 3 –i inputfilepath");
		exit(0);
	}
	if(bufferSize < receive2Shots*citizens+1){
		fprintf(stderr, "Must b >= tc+1 \n" );
		exit(0);
	}

	fprintf(stdout,"Welcome to the GTU344 clinic. Number of citizen to vaccinate c=%d t=%d \n",citizens,receive2Shots);
	processcount=nurses+vaccinators+citizens;
	childPids=(pid_t*)malloc(sizeof(int)*(processcount+1));
	bufferFull=(sem_t*)createSharedMemory(sizeof(sem_t));
	sem_init(bufferFull, 1, bufferSize);

	vaccineMutex=(sem_t*)createSharedMemory(sizeof(sem_t));
	sem_init(vaccineMutex, 1, 1);
	vaccineReady=(sem_t*)createSharedMemory(sizeof(sem_t));
	sem_init(vaccineReady, 1, 0);
	citizenMutex=(sem_t*)createSharedMemory(sizeof(sem_t));
	sem_init(citizenMutex, 1, 1);
	waitingCitizens=(sem_t*)createSharedMemory(sizeof(sem_t));
	sem_init(waitingCitizens, 1, 0);
	nurseMutex=(sem_t*)createSharedMemory(sizeof(sem_t));
	sem_init(nurseMutex, 1, 1);
	vaccine1=(int*)createSharedMemory(sizeof(int));
	vaccine2=(int*)createSharedMemory(sizeof(int));
	citizensTurn=(int*)createSharedMemory(sizeof(int)*(citizens));
	citizensLeftTurn=(int*)createSharedMemory(sizeof(int)*(citizens));
	citizensPIDS=(int*)createSharedMemory(sizeof(int)*(citizens));
	vaccinatorServiced=(int*)createSharedMemory(sizeof(int)*(vaccinators));
	nurseCounter=(int*)createSharedMemory(sizeof(int));
	nurseCounter[0]=nurses;
	for(int i=0;i<citizens;i++){
		citizensTurn[i]=-1;
	}
	for(int i=0;i<citizens;i++){
		citizensLeftTurn[i]=receive2Shots;
	}
	for(int i=0;i<citizens;i++){
		citizensPIDS[i]=0;
	}
	for(int i=0;i<vaccinators;i++){
		vaccinatorServiced[i]=0;
	}
	input=open(filePath,O_RDONLY);
	if(input<0){
		fprintf(stderr,"File error filePath ,\n");
		kill(getpid(),SIGINT);
	}
	childPids[0]=getpid();
	for (processcounter=0; processcounter < processcount ; processcounter++) {
		if ((childPids[processcounter+1] = fork()) == -1){
			fprintf(stderr, "ERROR ON FORKING\n" );
			exit(1); 
		}
		if (childPids[processcounter+1] == 0) {
			if(processcounter>nurses)
				close(input);
			if(processcounter>=processcount-citizens){
				citizensPIDS[processcounter-nurses-vaccinators]=getpid();
			}
			break;
		}
		else{
			aliveChildren++;
		}
	}
	if(processcounter < nurses){

		char readBuf[1]={};
		int old,new;
		//exit(0);
		while(1){
			sem_wait(nurseMutex);
			if(read(input,readBuf,sizeof(char))){
				sem_wait(bufferFull);
				sem_wait(vaccineMutex);
				old=checkReadyVaccines();
				if(readBuf[0]=='1'){
					vaccine1[0]++;
					printf("Nurse %d (pid = %d ) has brought vaccine 1: the clinic has %d vaccine1 and %d vaccine2.\n",processcounter,getpid(),vaccine1[0],vaccine2[0]);
					fflush(stdout);
					
				}
				else if(readBuf[0]=='2'){
					vaccine2[0]++;
					printf("Nurse %d (pid = %d ) has brought vaccine 2: the clinic has %d vaccine1 and %d vaccine2.\n",processcounter,getpid(),vaccine1[0],vaccine2[0]);
					fflush(stdout);
					
				}
				new=checkReadyVaccines();
				if(old!=new && new!=0){
					sem_post(vaccineReady);
				}
				sem_post(vaccineMutex);
			}
			else {
				nurseCounter[0]--;
				if(nurseCounter[0]==0){
					printf("Nurses have carried all vaccines to the buffer, terminating.\n");
				}
				free(childPids);
				childPids=NULL;
				sem_post(nurseMutex);
				fflush(stdout);
				close(input);
				exit(0);}
				sem_post(nurseMutex);

			}
			
			
		}

		else if(processcounter >= nurses && processcounter < nurses+vaccinators){
			returnSignalReceived=-1;

			int myDoses=0;
			while(allCitizensReady==0)
				sigsuspend(&interruptMask);

			while(1){

				sem_wait(vaccineReady);
				sem_wait(waitingCitizens);
				sem_wait(citizenMutex);
				for(int i=0;i<citizens;i++){
					if(citizensLeftTurn[i]!=0 && citizensTurn[i]==0){
						returnSignalReceived=0;
						myDoses++;
						setVaccinatorServices(myDoses,processcounter-nurses);
						printf("Vaccinator %d (pid=%d) is inviting citizen pid=%d to the clinic\n",processcounter-nurses,getpid(),citizensPIDS[i]);
						fflush(stdout);
						citizensTurn[i]=1;
						sem_wait(vaccineMutex);
						vaccine1[0]--;
						vaccine2[0]--;
						sem_post(vaccineMutex);
						sem_post(bufferFull);
						sem_post(bufferFull);
						kill(citizensPIDS[i],SIGUSR1);
						break;
					}
				}
				sem_post(citizenMutex);

				while(returnSignalReceived==0)
					sigsuspend(&interruptMask);
				//sleep(1);
				//sleep(2);
	
			}
	
		}
		else if(processcounter >= nurses+vaccinators && processcounter < nurses+vaccinators+citizens){
			citizensTurn[processcounter-nurses-vaccinators]=0;
			if(checkAllCitizens()){
				sendBeginSignalToVaccinators();
			}	
			//exit(0);
			while(citizensLeftTurn[processcounter-nurses-vaccinators]>0){	
				sem_post(waitingCitizens);

				while(turnStarted==0){
					sigsuspend(&act.sa_mask);
				}

				sem_wait(citizenMutex);
				citizensLeftTurn[processcounter-nurses-vaccinators]--;
				turnStarted=0;
				citizensTurn[processcounter-nurses-vaccinators]=0;
				
				sem_wait(vaccineMutex);
				if(citizensLeftTurn[processcounter-nurses-vaccinators]>0)
					printf("Citizen %d (pid=%d) is vaccinated for the %dst time: the clinic has %d vaccine1 and %d vaccine2 \n",processcounter-nurses-vaccinators,getpid(),receive2Shots-citizensLeftTurn[processcounter-nurses-vaccinators],vaccine1[0],vaccine2[0]);
				else{
					printf("Citizen %d (pid=%d) is vaccinated for the %dst time: the clinic has %d vaccine1 and %d vaccine2.The citizen is leaving. Remaining citizens to vaccinate: %d \n",processcounter-nurses-vaccinators,getpid(),receive2Shots-citizensLeftTurn[processcounter-nurses-vaccinators],vaccine1[0],vaccine2[0],getRemainingCitizens());
				}
				fflush(stdout);

				kill(mySignalSender,SIGUSR2);
				sem_post(vaccineMutex);
				sem_post(citizenMutex);
				
		}

		if(checkCitizensLeft()==0){
			for(int i=0;i<vaccinators;i++){
				kill(childPids[i+nurses+1],SIGTERM);
			}
		}
				free(childPids);
		childPids=NULL;
		exit(0);
		}
		else {
			int leftCitizens=citizens;
			while(aliveChildren>0)
				while((pid=waitpid(-1, &status, WNOHANG)) > 0){
					aliveChildren--;
					for(int i=0;i<citizens;i++){
						if(pid==citizensPIDS[i])
							leftCitizens--;
					}

					pid=0;
				}
			for(int i=0;i<vaccinators;i++){
				printf("Vaccinator %d (pid = %d) vaccinated %d doses.",i,childPids[i+1+nurses],vaccinatorServiced[i]);
			}
			free(childPids);
			childPids=NULL;
			sem_destroy(bufferFull);
			sem_destroy(vaccineMutex);
			sem_destroy(vaccineReady);
			sem_destroy(waitingCitizens);
			sem_destroy(citizenMutex);
			sem_destroy(nurseMutex);
			exit(0);	
		}

	}
	void* createSharedMemory(size_t size) {

		int protection = PROT_READ | PROT_WRITE;
		int visibility = MAP_SHARED | MAP_ANONYMOUS;

		return (void*)mmap(NULL, size+1, protection, visibility, -1, 0);
	}

	int checkReadyVaccines(){
		return vaccine1[0]<=vaccine2[0] ? vaccine1[0]:vaccine2[0];
	}
	int checkCitizensLeft(){
		
		int sum=0;
		for(int i=0;i<citizens;i++){
			sum+=citizensLeftTurn[i];
		}
		return sum;
	}
	void setVaccinatorServices(int myDoses,int myID){
		vaccinatorServiced[myID]++;
				

	}
	int checkAllCitizens(){
		for(int i=0;i<citizens;i++){
			if(citizensTurn[i]==-1){
				return 0;
			}
		}
		return 1;
	}
	void sendBeginSignalToVaccinators(){
		for(int i=nurses;i<nurses+vaccinators;i++){
			kill(childPids[i+1],SIGUSR1);
		}
	}
	int getRemainingCitizens(){
		int sum=0;
		for(int i=0;i<citizens;i++){
			if(citizensLeftTurn[i]>0)
			sum+=1;
		}
		return sum;
	}