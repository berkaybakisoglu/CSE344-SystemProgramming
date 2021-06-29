#include "SocketQueue.h"



struct SocketQueue* socketQueueFront=NULL;
struct SocketQueue* socketQueueRear=NULL;
pthread_mutex_t socketMutex = PTHREAD_MUTEX_INITIALIZER;
void socketEnqueue(int fd){

	struct SocketQueue* temp = (struct SocketQueue*)malloc(sizeof(struct SocketQueue));
	temp->sock =fd;
	temp->next =NULL;

	if(isSocketQueueEmpty()){
		pthread_mutex_lock(&socketMutex);
		socketQueueFront=socketQueueRear=temp;
		pthread_mutex_unlock(&socketMutex);
	}
	else
	{
		pthread_mutex_lock(&socketMutex);
		socketQueueRear->next = temp;
		socketQueueRear = temp;
		pthread_mutex_unlock(&socketMutex);
	}

}
int isSocketQueueEmpty(){
	pthread_mutex_lock(&socketMutex);
	if(socketQueueFront==NULL){
		pthread_mutex_unlock(&socketMutex);
		return 1;
		}

	else{
		pthread_mutex_unlock(&socketMutex);
		return 0;
		}
}
int socketDequeue(){
	
	int sock;
	if(isSocketQueueEmpty())
		return -1;
	pthread_mutex_lock(&socketMutex);
	struct SocketQueue* temp = socketQueueFront;
	sock=socketQueueFront->sock;
	socketQueueFront = socketQueueFront->next;
	free(temp);
	pthread_mutex_unlock(&socketMutex);

	return sock;
}
void destroySocketQueue(){
	while(!isSocketQueueEmpty()){
		socketDequeue();
	}
}
