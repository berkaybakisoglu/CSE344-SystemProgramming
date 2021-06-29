#ifndef SocketQueue_H
#define SocketQueue_H
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h> 
struct SocketQueue{
	int sock;
	struct SocketQueue* next;

};

void socketEnqueue(int fd);
int isSocketQueueEmpty();
int socketDequeue();
void destroySocketQueue();
#endif
