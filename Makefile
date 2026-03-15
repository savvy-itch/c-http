client: client.o tests.o
	gcc -o client client.o tests.o -lws2_32

client.o: client.c ./tests/tests.h
	gcc -c client.c

tests.o: ./tests/tests.c ./tests/tests.h
	gcc -c ./tests/tests.c

server: server.o globals.o
	gcc -o server server.o globals.o -lws2_32

server.o: server.c globals.h
	gcc -c server.c

globals.o: globals.c globals.h
	gcc -c globals.c