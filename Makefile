client_file := F4Client.c
clientcpu_file := F4ClientAuto.c
server_file := F4Server.c
server_exe := F4Server.o
client_exe := F4Client.o
clientcpu_exe := F4ClientAuto.o

all: client clientauto server

client: $(client_file)
	gcc -Wall $(client_file) -o $(client_exe)

clientauto: $(clientcpu_file)
	gcc -Wall $(clientcpu_file) -o $(clientcpu_exe)

server: $(server_file)
	gcc -Wall $(server_file) -o $(server_exe)
clean:
	rm $(server_exe) $(client_exe) $(clientcpu_exe)
