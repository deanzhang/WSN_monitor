CC = gcc
INCLUDE = -I./include/
LIBS = -lncurses
INSTALL_DIR = /usr/bin
EXEC = wsn_monitor

all: serial.o session.o
	$(CC) -o $(EXEC) serial.o session.o $(LIBS)
serial.o: src/serial.c
	$(CC) -Wall -Werror src/serial.c -c $(INCLUDE)
session.o: src/session.c
	$(CC) -Wall -Werror src/session.c -c $(INCLUDE)
install:
	cp -f EXEC $(INSTALL_DIR)/$(EXEC)
uninstall:
	rm -f $(INSTALL_DIR)/$(EXEC)

clean:
	rm -f $(EXEC) *.o
