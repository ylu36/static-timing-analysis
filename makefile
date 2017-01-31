OBJS = node.cpp
CC = g++ -w
DEBUG = -g -O3
CFLAGS = -Wall -c $(DEBUG)
LFLAGS = -Wall $(DEBUG)

parser : $(OBJS)
	$(CC) $(LFLAGS) $(OBJS) -o parser

clean:
	rm -f *.o parser
