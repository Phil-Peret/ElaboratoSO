client_file := F4Client.c
server_file := F4Server.c
server_exe := F4Server.o
client_exe := F4Client.o

all: client server

client: $(client_file)
	gcc $(client_file) -o $(client_exe)

server: $(server_file)
	gcc $(server_file) -o $(server_exe)

clean:
	rm Player1 Player2
