all: client	server	threadedserver
client: client.c
	gcc -g client.c -o client
server: eventdriven.c
	gcc -g eventdriven.c -o eventdriven
threadedserver: multithread.c
	gcc -g -pthread multithread.c -o threadedserver
killc:
	killall -9 client
killes:
	killall -9 eventdriven
killts:
	killall -9 threadedserver
clean:
	rm client threadedserver eventdriven
