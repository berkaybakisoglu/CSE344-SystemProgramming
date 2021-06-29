all :	server client

server:	server.o SocketQueue.o
	gcc server.o SocketQueue.o -pthread -o server
client: client.o
	gcc client.o -o client
client.o:
	gcc -c client.c -pthread -Wall
server.o: server.c
	gcc -c server.c -pthread -Wall 
SocketQueue.o:
	gcc -c SocketQueue.c -pthread -Wall
clean: 
	rm -rf *o server client
