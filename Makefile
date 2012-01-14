CC   = gcc
OPTS = -Wall

all: server libmfs.so

# this generates the target executables
server: server.o udp.o
	$(CC) -o server server.o udp.o  

libmfs.so: client.c udp.c
	gcc -Wall -Werror -shared -fPIC -o libmfs.so client.c udp.c

# this is a generic rule for .o files 
%.o: %.c 
	$(CC) $(OPTS) -c $< -o $@

clean:
	rm -f server.o udp.o client.o server libmfs.so




