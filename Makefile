CFLAGS = -Wall -Wextra
WFLAGS = -lws2_32

client: client.o tests.o
	gcc -o client client.o tests.o $(WFLAGS)

client.o: client.c ./tests/tests.h
	gcc -c client.c $(CFLAGS)

tests.o: ./tests/tests.c ./tests/tests.h
	gcc -c ./tests/tests.c $(CFLAGS)

server: server.o globals.o req.o res.o
	gcc -o server server.o globals.o req.o res.o $(WFLAGS)

server.o: server.c ./include/globals.h ./include/req.h
	gcc -c server.c $(CFLAGS)

res.o: res.c ./include/res.h ./include/req.h
	gcc -c res.c $(CFLAGS)

req.o: req.c ./include/req.h ./include/globals.h
	gcc -c req.c $(CFLAGS)

globals.o: globals.c ./include/globals.h
	gcc -c globals.c $(CFLAGS)