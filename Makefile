#
# To compile, type "make" or make "all"
# To remove files, type "make clean"
#
OBJS = server.o request.o segel.o client.o request_queue.o output.o
TARGET = server

CC = gcc
CFLAGS = -g -Wall

LIBS = -lpthread

.SUFFIXES: .c .o

all: $(TARGET) client output.cgi
	-mkdir -p public
	-cp output.cgi favicon.ico home.html public

$(TARGET): server.o request.o segel.o request_queue.o
	$(CC) $(CFLAGS) -o $(TARGET) server.o request.o segel.o request_queue.o $(LIBS)

client: client.o segel.o
	$(CC) $(CFLAGS) -o client client.o segel.o

output.cgi: output.o
	$(CC) $(CFLAGS) -o output.cgi output.o

.c.o:
	$(CC) $(CFLAGS) -o $@ -c $<

clean:
	-rm -f $(OBJS) $(TARGET) client output.cgi
	-rm -rf public