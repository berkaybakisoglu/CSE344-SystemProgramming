#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <dirent.h>
#include <sys/wait.h>

#define BOLDGREEN   "\033[1m\033[32m"
#define RESET   "\033[0m"
#define DEBUG 0
char* inputPath=NULL;
char* filename=NULL;
int bytes=-1;
char* fileType=NULL;
char* filePermissions=NULL;
int numLinks=-1;
int isAnyFileFound=0;
int wFlag = 0,fFlag = 0,bFlag=0,tFlag=0,pFlag=0,lFlag=0; // argument flags
void fileSearch(char *mainPath, int floor);
int checkFileConditions(char* path,char* curfilename);
void printFoundedFileName(char* filename);
int checkFilePermissions(mode_t fileMode);
int checkFileName(char* filename);
int checkFileSize(off_t fileSize);
int checkFileType(mode_t mode);
int checkNumberLinks(nlink_t fileLinks);
char* toLowercase(char* word);
int check(char* curfilename,char lastChar,int i);
void printFloor(int floor);
int closeSignal=0;

void signalHandling(int signum) {
    if (signum == 2) {
    	if(closeSignal==0){
        fprintf(stdout, "\n Process has been terminated. \n");
        closeSignal=1;}
        //else
        //	exit(0);
    }
}
int main(int argc, char** argv) {

    int opt;
    struct sigaction sigact;
    sigemptyset(&sigact.sa_mask);
    sigfillset(&sigact.sa_mask);
    sigact.sa_flags = 0;
    sigact.sa_handler = signalHandling;
    for (int i = 1; i < 32; i++) {
        sigaction(i, &sigact, NULL);
    }
    while ((opt = getopt(argc, argv, "w:f:b:t:p:l:")) != -1) {
        switch (opt) {
            case 'w':
                wFlag = 1;
                inputPath = optarg;
                break;
            case 'f':
                filename = optarg;
                if(filename!=NULL){
                	for(int i=0;i<strlen(filename);i++)
						filename[i]=tolower(filename[i]);
			}
                fFlag = 1;
                break;
            case 'b':
                bytes = atoi(optarg);
                bFlag = 1;
                break;
            case 't':
                fileType = optarg;
                tFlag = 1;
                break;
            case 'p':
                filePermissions = optarg;
                if(strlen(filePermissions)!=9){
                	fprintf(stderr,"File permission parameter should be 9 characters,exit.\n");
                	exit(0);
                }
                pFlag = 1;
                break;
            case 'l':
                numLinks = atoi(optarg);
                if(numLinks<0){
                	fprintf(stderr,"Number of links parameter should be 0 or more,exit.\n");
                	exit(0);}
                lFlag = 1;
                break;
            default: /* '?' */
                fprintf(stdout, "Usage: /hw1 (mandatory)-w path (optional)-f filename -b bytes -t filetype -p permissions -l numberofLinks");
                exit(0);
        }
    }
    if(wFlag!=1){
        fprintf(stderr,"W flag and pathname is mandatory,program exits.");
        exit(0);
    }
    if(lFlag+pFlag+tFlag+bFlag+fFlag < 1)
        fprintf(stderr,"At least parameter has to deployed,program exits.");
    if(DEBUG)
    	printf("%s,%s,%d,%s,%s\n",inputPath,filename,bytes,fileType,filePermissions);

    fileSearch(inputPath,0);
    if(!isAnyFileFound)
    	printFoundedFileName("No file has found with search criteria");

}
void fileSearch(char *mainPath, int floor)
{
    char path[PATH_MAX];
    struct dirent* dp;
    DIR* dir;
    if(!(dir=opendir(mainPath))){
    	fprintf(stderr,"Error on opening directory\n");
    	return;
    }
    if(floor==0){
    	printf("%s\n",mainPath);
    }
    while ((dp=readdir(dir))!=NULL)
    {
    	if(closeSignal){
    		closedir(dir);
    		return;
    	}
        if (strcmp(dp->d_name,".")!=0 && strcmp(dp->d_name,"..")!=0)
        {
            strcpy(path,mainPath);
            strcat(path,"/");
            strcat(path,dp->d_name);
            if (dp->d_type==DT_DIR){
				printFloor(floor);
            	if(checkFileConditions(path,dp->d_name)){
            		printFoundedFileName(dp->d_name);
            	}
            	else
            		printf("%s\n",dp->d_name);
            	fileSearch(path, floor + 1);
            }
            else{
            	if(checkFileConditions(path,dp->d_name)){
            		printFloor(floor);
            		printFoundedFileName(dp->d_name);
            	}
            }
        }
    }
    closedir(dir);
}

void printFoundedFileName(char* filename){
	isAnyFileFound++;
    printf( BOLDGREEN "%s",filename);
    printf( BOLDGREEN " \n" RESET);
}
void printFloor(int floor){
	printf("|--");
    for(int i=0;i<floor;i++)
        printf("--");
}
int checkFileConditions(char* path,char* curfilename){
	struct stat fileStat;
	char* copyfilename;
	if(lstat(path,&fileStat)>=0){
		if(fFlag==1){
			copyfilename=(char*)malloc((strlen(curfilename)+1)*sizeof(char) );
			for(int i=0;i<strlen(curfilename);i++)
				copyfilename[i]=tolower(curfilename[i]);
			copyfilename[strlen(curfilename)]='\0';
		}
				
			if(DEBUG)
				printf("\n Filesize=%d \n FileName=%d\n fileType=%d \n numberofLinks=%d\n permissions= %d \n",checkFileSize(fileStat.st_size),checkFileName(copyfilename),checkFileType(fileStat.st_mode),checkNumberLinks(fileStat.st_nlink),checkFilePermissions(fileStat.st_mode) );
    	if(checkFileName(copyfilename)&&checkFileSize(fileStat.st_size) && checkFileType(fileStat.st_mode) && checkNumberLinks(fileStat.st_nlink) && checkFilePermissions(fileStat.st_mode)){
    		if(fFlag==1)
    			free(copyfilename);
    		return 1;
    	}
	}
	else{
		fprintf(stderr,"Due to lstat error,exit.\n");
	}
	return 0;		
}
int checkFilePermissions(mode_t fileMode){
	char permissions[9];
	if(pFlag==1){
		permissions[0]=(fileMode & S_IRUSR) ? 'r' : '-';
		permissions[1]=(fileMode & S_IRUSR) ? 'w' : '-';
		permissions[2]=(fileMode & S_IXUSR) ? 'x' : '-';
		permissions[3]=(fileMode & S_IRGRP) ? 'r' : '-';
		permissions[4]=(fileMode & S_IWGRP) ? 'w' : '-';
		permissions[5]=(fileMode & S_IXGRP) ? 'x' : '-';
		permissions[6]=(fileMode & S_IROTH) ? 'r' : '-';
		permissions[7]=(fileMode & S_IWOTH) ? 'w' : '-';
		permissions[8]=(fileMode & S_IXOTH) ? 'x' : '-';
		for(int i=0;i<strlen(filePermissions);i++){
			if(permissions[i]!=filePermissions[i])
				return 0;
		}
	}
	return 1;
}
int checkFileName(char* curfilename){
	int j=0;
	int i=0;
	if(fFlag==1){
	int curfilelen=strlen(curfilename);
	int filelen=strlen(filename);
	
		if(1+curfilelen<filelen)
			return 0;
		for(i=0,j=0;i<strlen(filename);i++,j++){
			if(filename[i]=='+' && i==0)
				return 0;
			if(filename[i]!=curfilename[j]){
				if(filename[i]=='+'){
					j=check(curfilename,filename[i-1],j);
				}
				else
					return 0;
		}

	}
	if(DEBUG)
		printf("%d=%d %d=%d \n",filelen,i,curfilelen,j);
	if(j==curfilelen && i==filelen)
		return 1;
	else
		return 0;
}
	return 1;

}
int check(char* curfilename,char lastChar,int j){
	int i=0;
	for(i=j;i<strlen(curfilename);i++){
		if(curfilename[i]!=lastChar)
			return i-1;
	}
	return i;
}
int checkNumberLinks(nlink_t fileLinks){
	if(DEBUG)
		printf("\n\n Number of Links: %ju\n\n", fileLinks);
	if(lFlag==1){
		if(numLinks==fileLinks)
			return 1;
		return 0;
	}
	return 1;
}
int checkFileSize(off_t fileSize){
	if(DEBUG)
		printf("File Size: \t\t%ld bytes and %d \n", fileSize,bytes);
	if(bFlag==1){
		if(fileSize==bytes)
			return 1;
		return 0;
	}
	else 
		return 1;
}
int checkFileType(mode_t mode){
	char ftype;
	if(tFlag==1){
		if(S_ISREG(mode)){
			ftype='f';
		}
		else if(S_ISDIR(mode)){
			ftype='d';
		}
		else if(S_ISCHR(mode)){
			ftype='c';
		}
		else if(S_ISBLK(mode)){
			ftype='b';
		}
		else if(S_ISFIFO(mode)){
			ftype='p';
		}
		else if(S_ISLNK(mode)){
			ftype='l';
		}
		else if(S_ISSOCK(mode)){
			ftype='s';
		}
		else 
			return 0;
		if(ftype==fileType[0])
			return 1;
		return 0;
	}

		return 1;
}