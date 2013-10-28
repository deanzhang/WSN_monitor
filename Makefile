CC = gcc
INCLUDE = -I./include/
LIBS = 
INSTALL_DIR = /usr/bin
EXEC = wsn_monitor

all: serial.o
	$(CC) -o $(EXEC) serial.o $(LIBS)
serial.o: src/serial.c
	$(CC) -Wall -Werror src/serial.c -c $(INCLUDE)
install:
	cp -f EXEC $(INSTALL_DIR)/$(EXEC)
uninstall:
	rm -f $(INSTALL_DIR)/$(EXEC)

clean:
	rm -f $(EXEC) *.o
