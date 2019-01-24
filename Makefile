CFLAGS=-g -O2 -Wall -fsanitize=address -DDICTIONARY='"$(shell pwd)/words"'
CXXFLAGS=$(CFLAGS)
LDFLAGS=-pthread $(CFLAGS)

all: line words

line: line.o

%.o: %.c %.h

words:
	tr '\n' '\0' </usr/share/dict/words >words
