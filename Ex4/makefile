CC = gcc
CFLAGS = -Wall

all : myPing sniffer

myPing: myPing.o
	$(CC) $(CFLAGS) myPing.o -o myPing

myPing.o: myPing.c
	$(CC) $(CFLAGS) -c myPing.c -o myPing.o

sniffer: sniffer.o
	$(CC) $(CFLAGS) sniffer.o -lpcap -o sniffer

sniffer.o: sniffer.c
	$(CC) $(CFLAGS) -c sniffer.c -o sniffer.o

.PHONY: clean myPing sniffer

clean:
	rm *.o myPing
