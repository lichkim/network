myserver: http_server.o
	gcc -o myserver http_server.o

http_server.o: http_server.c
	gcc -c -o http_server.o http_server.c
