CC = gcc 
INCLUDES = -Iinclude
CFLAGS = -g -pthread -no-pie $(INCLUDES)
LIBDIR = obj
LIBS = $(LIBDIR)/libfdr.a $(LIBDIR)/sockettome.o
EXECUTABLES = chat_server

all: $(EXECUTABLES)

.SUFFIXES: .c .o
.c.o:
	$(CC) $(CFLAGS) -c $*.c

chat_server: chat_server.o
	$(CC) $(CFLAGS) -o bin/chat_server chat_server.o $(LIBS)

#make clean will rid your directory of the executable,
#object files, and any core dumps you've caused
clean:
	rm -rf core $(EXECUTABLES) *.o tmp*.txt output-*.txt

