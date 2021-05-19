CC=gcc
CFLAGS=-g -Wall
TARGET1=server
TARGET2=client

OBJECT1=send_packet.o udpserver.o
OBJECT2=send_packet.o udpclient.o

all: $(TARGET1) $(TARGET2)

$(TARGET1) : $(OBJECT1)
	$(CC) -o $(TARGET1) send_packet.o udpserver.o
	
$(TARGET2) : $(OBJECT2)
	$(CC) -o $(TARGET2) send_packet.o udpclient.o

send_packet.o : send_packet.c
	$(CC) -c -o send_packet.o send_packet.c

udpserver.o : udpserver.c
	$(CC) -c -o udpserver.o udpserver.c

udpclient.o : udpclient.c
	$(CC) -c -o udpclient.o udpclient.c

clean:
	rm -f *.o
	rm -f $(TARGET1)
	rm -f $(TARGET2)
